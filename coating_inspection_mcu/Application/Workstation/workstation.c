#include "workstation.h"
#include "output_policy.h"
#include "health.h"
#include <string.h>

/* 故障按此优先级升级；首发故障单独保存，不被后续更严重故障覆盖。 */
static unsigned FaultRank(uint16_t f)
{
    switch (f) {
    case FAULT_TASK: return 6;
    case FAULT_RX: return 5;
    case FAULT_REMOVED: return 4;
    case FAULT_TIMEOUT: return 3;
    case FAULT_ABORT: return 2;
    case FAULT_UNSYNC: return 1;
    default: return 0;
    }
}
void WS_Fault(Workstation *s, uint16_t cause, uint32_t now)
{
    /* 首次关门、故障升级或取消在途 token 时推进安全代次；溢出时请求复位。 */
    if (!s->fault_gate || FaultRank(cause)>FaultRank(s->fault) || s->token) {
        if (s->safety_generation==UINT32_MAX) s->reset_requested=true;
        else ++s->safety_generation;
    }
    if (!s->first_fault) {
        s->first_fault=cause; s->first_fault_tick=now; s->first_fault_token=s->token;
    }
    if (FaultRank(cause)>FaultRank(s->fault)) s->fault=cause;
    s->state=WS_FAULT; s->token=0; s->fault_gate=true;
    s->outputs=OutputPolicy_Resolve(0,true);
}
void WS_Init(Workstation *s, uint16_t reason, uint32_t now)
{
    memset(s,0,sizeof(*s)); s->state=WS_BOOT; s->reset_reason=reason;
    s->rx_healthy=true; s->sync_tick=now; s->buttons_open=true;
}
bool WS_HostFresh(const Workstation *s, uint32_t now)
{ return s->epoch!=0 && s->heartbeat_valid && Elapsed32(now,s->heartbeat_tick)<HOST_TIMEOUT_MS; }
void WS_Heartbeat(Workstation *s, uint16_t value, uint32_t now)
{
    /* 只有心跳值变化才刷新时间；重复写入相同值不能掩盖上位机业务停滞。 */
    if (value!=s->host_heartbeat) {
        s->host_heartbeat=value; s->heartbeat_tick=now; s->heartbeat_valid=true;
    }
}
static void ButtonEdge(Workstation *s, uint16_t code, uint32_t now)
{
    /* 同时只锁存一个按钮事件；未重新开放时的按下沿仅累计忙计数。 */
    if (!s->buttons_open || s->event_code) { SaturatingIncrement(&s->button_busy); return; }
    if (s->event_seq==UINT32_MAX) {
        WS_Fault(s,FAULT_TASK,now); s->reset_requested=true; return;
    }
    ++s->event_seq; s->event_code=code; s->buttons_open=false;
}
void WS_Inputs(Workstation *s, uint16_t inputs, uint32_t now)
{
    uint16_t old=s->inputs;
    s->inputs=inputs & 7U;
    /* ACK 已清事件、对应空事件快照发完、距 ACK 满 1200 ms 且双按钮释放后才重新开放。 */
    if (s->rearm_pending && s->rearm_response &&
        Elapsed32(now,s->rearm_tick)>=BUTTON_HOLD_MS && !(s->inputs & 6U)) {
        s->buttons_open=true; s->rearm_pending=false;
    }
    if ((s->inputs & INPUT_ACTION) && !(old & INPUT_ACTION)) ButtonEdge(s,1,now);
    if ((s->inputs & INPUT_RESET) && !(old & INPUT_RESET)) ButtonEdge(s,2,now);
    if (!(s->inputs & INPUT_PRESENT)) {
        /* 检测中离位是故障；待检测或已完成时离位只清结果并回到空闲。 */
        if (s->state==WS_INSPECTING) WS_Fault(s,FAULT_REMOVED,now);
        else if (s->state==WS_COMPLETED || s->state==WS_READY) {
            s->state=WS_IDLE; s->token=0; s->outputs=0;
        }
    } else if (s->state==WS_IDLE) s->state=WS_READY;
}
void WS_Protect(Workstation *s, uint16_t health, bool rx, uint32_t now)
{
    /* 保护会锁存故障门控；链路或心跳恢复本身不清故障，必须通过健康检查后 RESET。 */
    s->health=health & HEALTH_ALL; s->rx_healthy=rx;
    if (health!=HEALTH_ALL) WS_Fault(s,FAULT_TASK,now);
    else if (!rx && (s->state==WS_INSPECTING || s->state==WS_COMPLETED))
        WS_Fault(s,FAULT_RX,now);
    if (!s->epoch) WS_Fault(s,FAULT_UNSYNC,now);
    else if ((s->heartbeat_valid && !WS_HostFresh(s,now)) ||
             (!s->heartbeat_valid && Elapsed32(now,s->sync_tick)>=HOST_TIMEOUT_MS))
        WS_Fault(s,FAULT_TIMEOUT,now);
    s->outputs=OutputPolicy_Resolve(s->outputs,s->fault_gate);
}
static bool Healthy(const Workstation *s, uint32_t now)
{ return !s->reset_requested && s->health==HEALTH_ALL && s->rx_healthy && WS_HostFresh(s,now); }
void WS_Step(Workstation *s, uint16_t inputs, uint16_t health, bool rx, uint32_t now)
{
    bool inspecting=s->state==WS_INSPECTING;
    /* 先保护再处理离位，避免同一周期的正常结果撤销掩盖接收故障。 */
    WS_Protect(s,health,rx,now);
    WS_Inputs(s,inputs,now);
    if (inspecting && !(inputs & INPUT_PRESENT)) WS_Fault(s,FAULT_REMOVED,now);
}
static uint16_t Execute(Workstation *s, const WorkCommand *c, uint32_t now)
{
    switch (c->opcode) {
    case CMD_SYNC:
        /* 新会话使旧队列命令失效；同步后仍保持故障，等待变化心跳和 RESET。 */
        if (c->token || c->arg) return ACK_PARAMETER;
        if (s->safety_generation==UINT32_MAX) { WS_Fault(s,FAULT_TASK,now); return ACK_HEALTH; }
        ++s->safety_generation;
        s->epoch=c->epoch; s->sync_tick=now; s->heartbeat_valid=false;
        s->token=0; s->event_code=0; s->buttons_open=true;
        s->rearm_pending=false; s->rearm_response=false;
        WS_Fault(s,FAULT_UNSYNC,now); return ACK_OK;
    case CMD_START:
        if (!c->token || c->arg) return ACK_PARAMETER;
        if (s->state!=WS_READY || !(s->inputs & INPUT_PRESENT)) return ACK_STATE;
        if (!Healthy(s,now)) return ACK_HEALTH;
        s->token=c->token; s->state=WS_INSPECTING; s->outputs=0; return ACK_OK;
    case CMD_RESULT:
        /* 结果只接受当前检测 token，并再次检查工件在位、任务及上位机健康。 */
        if (c->arg!=OUTPUT_PASS && c->arg!=OUTPUT_FAIL) return ACK_PARAMETER;
        if (s->state!=WS_INSPECTING) return ACK_STATE;
        if (!c->token || c->token!=s->token) return ACK_TOKEN;
        if (!(s->inputs & INPUT_PRESENT)) return ACK_STATE;
        if (!Healthy(s,now)) return ACK_HEALTH;
        s->state=WS_COMPLETED;
        s->outputs=OutputPolicy_Resolve((uint16_t)c->arg,s->fault_gate); return ACK_OK;
    case CMD_ACK_EVENT:
        /* 先清除锁存事件并启动等待；按钮重新开放还需要完整回复的发送确认。 */
        if (!s->event_code || c->arg!=s->event_seq) return ACK_PARAMETER;
        s->event_code=0; s->rearm_pending=true; s->rearm_response=false;
        s->rearm_tick=now; s->rearm_epoch=c->epoch; s->rearm_seq=c->seq; return ACK_OK;
    case CMD_RESET:
        /* RESET 只解除健康工位的故障锁存，最终进入 READY 或 IDLE，不启动检测。 */
        if (c->token || c->arg) return ACK_PARAMETER;
        if (s->state!=WS_FAULT) return ACK_STATE;
        if (!Healthy(s,now)) return ACK_HEALTH;
        s->fault_gate=false; s->fault=FAULT_NONE; s->token=0; s->outputs=0;
        s->first_fault=0; s->first_fault_tick=0; s->first_fault_token=0;
        s->state=(s->inputs & INPUT_PRESENT) ? WS_READY : WS_IDLE; return ACK_OK;
    case CMD_ABORT:
        if (c->arg) return ACK_PARAMETER;
        if (s->token && s->token!=c->token) return ACK_TOKEN;
        WS_Fault(s,FAULT_ABORT,now); return ACK_OK;
    default: return ACK_PARAMETER;
    }
}
uint16_t WS_QueuedCommand(Workstation *s, const WorkCommand *c, uint32_t generation, uint32_t now)
{
    uint16_t status;
    bool identity_ok;
    if (s->cache_valid && c->epoch==s->cached.epoch && c->seq==s->cached.seq) {
        /* 最近一次命令重试直接复用成功或拒绝结果；同身份但内容变化则拒绝。 */
        status=Command_Equal(c,&s->cached) ? s->cached_status : ACK_IDENTITY;
    } else {
        /* 新 SYNC 的 epoch 必须递增；普通命令须属于当前会话且 seq 严格递增。 */
        identity_ok=c->epoch!=0 && c->seq!=0 &&
            ((c->opcode==CMD_SYNC && c->epoch>s->epoch) ||
             (c->opcode!=CMD_SYNC && c->epoch==s->epoch &&
              c->seq>s->last_seq));
        if (!identity_ok) status=ACK_IDENTITY;
        else {
            /* SYNC 可建立新代次；普通命令只在入队代次仍有效时执行。 */
            status=(c->opcode==CMD_SYNC || generation==s->safety_generation) ?
                Execute(s,c,now) : ACK_STATE;
            if (c->epoch==s->epoch) s->last_seq=c->seq;
            /* 业务拒绝也缓存，防止同一命令在条件变化后重试产生不同效果。 */
            s->cached=*c; s->cached_status=status; s->cache_valid=true;
        }
    }
    s->ack_epoch=c->epoch; s->ack_seq=c->seq; s->ack_status=status;
    if (status!=ACK_OK) SaturatingIncrement(&s->business_rejects);
    s->outputs=OutputPolicy_Resolve(s->outputs,s->fault_gate);
    return status;
}
uint16_t WS_Command(Workstation *s, const WorkCommand *c, uint32_t now)
{ return WS_QueuedCommand(s,c,s->safety_generation,now); }
void WS_SnapshotSent(Workstation *s, uint32_t epoch, uint32_t seq,
                     uint16_t status, uint16_t code)
{
    /* 旧快照、部分寄存器读或发送失败均不能确认本次按钮事件已处理。 */
    if (s->rearm_pending && epoch==s->rearm_epoch && seq==s->rearm_seq &&
        status==ACK_OK && code==0) s->rearm_response=true;
}
static void Put32(uint16_t *words, uint32_t value)
{ words[0]=(uint16_t)(value>>16); words[1]=(uint16_t)value; }
void WS_Snapshot(const Workstation *s, uint32_t now, uint16_t w[INPUT_REG_COUNT])
{
    /* 固定 22 字输入寄存器布局；32 位字段高字在前，与上位机协议保持一致。 */
    w[0]=s->state; w[1]=s->inputs; w[2]=s->outputs; w[3]=s->fault;
    w[4]=s->health; w[5]=s->mcu_heartbeat; Put32(w+6,now); Put32(w+8,s->token);
    Put32(w+10,s->ack_seq); w[12]=s->ack_status; Put32(w+13,s->event_seq);
    w[15]=s->event_code; w[16]=s->reset_reason; Put32(w+17,s->comm_errors);
    w[19]=PROTOCOL_VERSION; Put32(w+20,s->ack_epoch);
}
