#include "app_runtime.h"
#include "queue.h"
#include "board_config.h"
#include "bsp_reset.h"
#include "bsp_rs485.h"
#include "bsp_watchdog.h"
#include "bsp_UART.h"
#include "bsp_board.h"
#include "output_manager.h"
#include "debounce.h"
#include "modbus.h"
#include "health.h"
#include "diagnostics.h"
#include <stdio.h>
#include <string.h>

Workstation g_workstation;
volatile uint16_t g_stack_watermark[3];
volatile uint32_t g_task_cycles[3];
static TaskProgress progress;
static TaskHandle_t commTask,workTask,healthTask;
static StaticTask_t commTCB,workTCB,healthTCB;
static StackType_t commStack[768],workStack[512],healthStack[256];
typedef struct { WorkCommand command; uint32_t generation; } QueuedCommand;
static StaticQueue_t commandQueueControl;
static uint8_t commandQueueStorage[COMMAND_QUEUE_CAPACITY*sizeof(QueuedCommand)];
static QueueHandle_t commandQueue;
static Diagnostics diagnostics;
static volatile bool schedulerStarted;
static uint16_t publishedState, publishedOutput, publishedFault;
static uint32_t publishedEventSeq;
static uint16_t publishedEventCode;
static RtuFrame frame;
static ModbusReply response;
static bool awaitingTX;
#if COATING_DEBUG_INJECTION
static volatile bool pauseComm,pauseWork,pauseHealth,forceRX;
static volatile int forcedInputs=-1;
#endif

static void Log(uint16_t code, uint32_t tick, uint32_t a, uint32_t b, uint32_t c)
{ (void)Diagnostics_Push(&diagnostics,(Diagnostic){tick,a,b,c,code}); }
/* Every caller owns taskENTER_CRITICAL. State, output and fault gate publish together. */
static void Commit(uint32_t now)
{
    Workstation *s=&g_workstation;
    s->outputs=OutputManager_Commit(s->outputs,s->fault_gate);
    if (s->state!=publishedState) {
        Log(DIAG_STATE,now,publishedState,s->state,s->token); publishedState=s->state;
    }
    if (s->outputs!=publishedOutput) {
        Log(DIAG_OUTPUT,now,publishedOutput,s->outputs,s->token); publishedOutput=s->outputs;
    }
    if (s->fault!=publishedFault) {
        Log(DIAG_FIRST_FAULT,now,s->fault,s->first_fault_token,s->first_fault_tick);
        publishedFault=s->fault;
    }
    if (s->event_seq!=publishedEventSeq || s->event_code!=publishedEventCode) {
        Log(DIAG_EVENT,now,s->event_seq,s->event_code,s->inputs);
        publishedEventSeq=s->event_seq; publishedEventCode=s->event_code;
    }
}
static bool RXHealthy(void)
{
#if COATING_DEBUG_INJECTION
    if (forceRX) return false;
#endif
    return BSP_RS485_Healthy();
}
static void Protect(uint32_t now)
{ WS_Protect(&g_workstation,Health_Evaluate(&progress,now),RXHealthy(),now); }
static void Snapshot(void *ctx, uint16_t *words)
{
    (void)ctx;
    taskENTER_CRITICAL(); WS_Snapshot(&g_workstation,HAL_GetTick(),words); taskEXIT_CRITICAL();
}
static bool Holding(void *ctx, uint16_t address, uint16_t count, uint16_t *words)
{
    uint16_t cached[COMMAND_REG_COUNT]; bool result=false; (void)ctx;
    taskENTER_CRITICAL();
    if (address==0x0100 && count==1) { words[0]=g_workstation.host_heartbeat; result=true; }
    else if (address>=0x0110 && address<=0x0118 && count<=0x0119-address) {
        Command_Encode(&g_workstation.cached,cached);
        memcpy(words,cached+(address-0x0110),count*sizeof(*words)); result=true;
    }
    taskEXIT_CRITICAL(); return result;
}
static void Heartbeat(void *ctx, uint16_t value)
{
    (void)ctx; taskENTER_CRITICAL();
    WS_Heartbeat(&g_workstation,value,HAL_GetTick()); taskEXIT_CRITICAL();
}
static bool Enqueue(void *ctx, const WorkCommand *command)
{
    QueuedCommand item; BaseType_t result; (void)ctx;
    taskENTER_CRITICAL(); item.command=*command; item.generation=g_workstation.safety_generation;
    result=xQueueSend(commandQueue,&item,0); taskEXIT_CRITICAL(); return result==pdPASS;
}
static void CommError(void *ctx)
{
    (void)ctx; taskENTER_CRITICAL(); SaturatingIncrement(&g_workstation.comm_errors); taskEXIT_CRITICAL();
}
static const ModbusService service={NULL,Snapshot,Holding,Heartbeat,Enqueue,CommError};
static void Deliver(void *ctx, const uint8_t *data, size_t length)
{
    (void)ctx; Modbus_Handle(data,length,&service,&response);
    if (response.length) {
        awaitingTX=BSP_RS485_Send(response.data,response.length);
        if (!awaitingTX) CommError(NULL);
    }
}
void BSP_RS485_WakeFromISR(void)
{
    if (schedulerStarted && commTask) {
        BaseType_t wake=pdFALSE; vTaskNotifyGiveFromISR(commTask,&wake); portYIELD_FROM_ISR(wake);
    }
}
static void DrainDiagnostics(void)
{
    unsigned budget=2;
    while (budget--) {
        Diagnostic event; char text[128]; bool available; int length;
        taskENTER_CRITICAL(); available=Diagnostics_Pop(&diagnostics,&event); taskEXIT_CRITICAL();
        if (!available) break;
        length=snprintf(text,sizeof(text),"v=" FW_VERSION " t=%lu code=%u a=%lu b=%lu c=%lu\r\n",
            (unsigned long)event.tick,(unsigned)event.code,(unsigned long)event.a,
            (unsigned long)event.b,(unsigned long)event.c);
        if (length>0 && (size_t)length<sizeof(text)) (void)BSP_DebugWrite(text,(size_t)length);
    }
}
#if COATING_DEBUG_INJECTION
static void DebugLine(const char *line)
{
    if (!strcmp(line,"inputs auto")) forcedInputs=-1;
    else if (!strncmp(line,"inputs ",7) && strlen(line)==8 && line[7]>='0' && line[7]<='7')
        forcedInputs=line[7]-'0';
    else if (!strcmp(line,"pause comm 1")) pauseComm=true;
    else if (!strcmp(line,"pause comm 0")) pauseComm=false;
    else if (!strcmp(line,"pause work 1")) pauseWork=true;
    else if (!strcmp(line,"pause work 0")) pauseWork=false;
    else if (!strcmp(line,"pause health 1")) pauseHealth=true;
    else if (!strcmp(line,"pause health 0")) pauseHealth=false;
    else if (!strcmp(line,"rx_fault 1")) forceRX=true;
    else if (!strcmp(line,"rx_fault 0")) forceRX=false;
    else if (!strcmp(line,"diag")) {
        char text[160]; int n;
        n=snprintf(text,sizeof(text),"diag drop=%lu uartdrop=%lu stack=%u,%u,%u cycles=%lu,%lu,%lu\r\n",
            (unsigned long)diagnostics.dropped,(unsigned long)g_debug_tx_dropped,
            g_stack_watermark[0],g_stack_watermark[1],g_stack_watermark[2],
            (unsigned long)g_task_cycles[0],(unsigned long)g_task_cycles[1],(unsigned long)g_task_cycles[2]);
        if (n>0 && (size_t)n<sizeof(text)) (void)BSP_DebugWrite(text,(size_t)n);
    } else {
        static const char help[]="inputs 0..7|auto; pause comm|work|health 0|1; rx_fault 0|1; diag\r\n";
        (void)BSP_DebugWrite(help,sizeof(help)-1);
    }
}
static void PumpDebug(void)
{
    static char line[64]; static unsigned used; static bool discard;
    static uint32_t observedDrops;
    unsigned budget=64; uint8_t byte;
    if (observedDrops!=g_debug_rx_dropped) {
        observedDrops=g_debug_rx_dropped; used=0; discard=true;
    }
    while (budget-- && BSP_DebugRead(&byte)) {
        if (byte=='\n') {
            if (!discard) { line[used]=0; DebugLine(line); }
            used=0; discard=false;
        } else if (byte!='\r') {
            if (used<sizeof(line)-1) line[used++]=(char)byte;
            else discard=true;
        }
    }
}
#endif
static void CommTask(void *arg)
{
    (void)arg; schedulerStarted=true; Rtu_Init(&frame);
    for (;;) {
        unsigned budget=128; RxRecord record; uint32_t errors;
#if COATING_DEBUG_INJECTION
        PumpDebug();
        if (pauseComm) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
#endif
        errors=BSP_RS485_Errors();
        if (errors) {
            uint32_t now=HAL_GetTick();
            taskENTER_CRITICAL();
            if (UINT32_MAX-g_workstation.comm_errors<errors) g_workstation.comm_errors=UINT32_MAX;
            else g_workstation.comm_errors+=errors;
            if (g_workstation.state==WS_INSPECTING || g_workstation.state==WS_COMPLETED)
                WS_Fault(&g_workstation,FAULT_RX,now);
            Log(DIAG_RX,now,errors,g_workstation.state,0); Commit(now); taskEXIT_CRITICAL();
            Rtu_Invalidate(&frame,BSP_Micros()); BSP_RS485_Recover();
        }
        if (!BSP_RS485_Healthy()) BSP_RS485_Recover();
        if (awaitingTX) {
            int result=BSP_RS485_TxResult();
            if (result) {
                if (result>0 && response.full_snapshot) {
                    taskENTER_CRITICAL();
                    WS_SnapshotSent(&g_workstation,response.ack_epoch,response.ack_seq,
                                     response.ack_status,response.event_code);
                    taskEXIT_CRITICAL();
                }
                awaitingTX=false;
            }
        }
        while (!awaitingTX && budget-- && BSP_RS485_Read(&record)) {
            if (record.kind==RX_BYTE) Rtu_Byte(&frame,record.byte,record.us,Deliver,CommError,NULL);
            else if (record.kind==RX_SILENCE) Rtu_Idle(&frame,record.us,Deliver,CommError,NULL);
            else Rtu_Invalidate(&frame,record.us);
        }
        DrainDiagnostics();
        taskENTER_CRITICAL(); progress.comm=HAL_GetTick(); ++g_task_cycles[0]; taskEXIT_CRITICAL();
        g_stack_watermark[0]=(uint16_t)uxTaskGetStackHighWaterMark(NULL);
        /* A real blocked tick gives priority-3 WorkTask CPU under a continuous RX flood. */
        vTaskDelay(1);
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(9));
    }
}
static uint16_t ReadInputs(void)
{
    uint16_t flags=0;
#if COATING_DEBUG_INJECTION
    int injected=forcedInputs;
    if (injected>=0) return (uint16_t)injected;
#endif
    if (HAL_GPIO_ReadPin(BOARD_PRESENT_PORT,BOARD_PRESENT_PIN)==BOARD_PRESENT_ACTIVE) flags|=1;
    if (HAL_GPIO_ReadPin(BOARD_ACTION_PORT,BOARD_ACTION_PIN)==BOARD_ACTION_ACTIVE) flags|=2;
    if (HAL_GPIO_ReadPin(BOARD_RESET_PORT,BOARD_RESET_PIN)==BOARD_RESET_ACTIVE) flags|=4;
    return flags;
}
static void WorkTask(void *arg)
{
    Debounce inputs[3]; TickType_t wake=xTaskGetTickCount(); unsigned i; (void)arg;
    for (i=0;i<3;++i) Debounce_Init(inputs+i,false,HAL_GetTick());
    for (;;) {
        uint32_t now=HAL_GetTick(); uint16_t raw=ReadInputs(),stable=0; QueuedCommand item;
        unsigned budget=COMMAND_QUEUE_CAPACITY;
#if COATING_DEBUG_INJECTION
        if (pauseWork) { vTaskDelayUntil(&wake,pdMS_TO_TICKS(10)); continue; }
#endif
        for (i=0;i<3;++i) {
            (void)Debounce_Update(inputs+i,(raw & (1U<<i))!=0,now);
            if (inputs[i].stable) stable|=(uint16_t)(1U<<i);
        }
        taskENTER_CRITICAL();
        now=HAL_GetTick();
        WS_Step(&g_workstation,stable,Health_Evaluate(&progress,now),RXHealthy(),now);
        Commit(now); taskEXIT_CRITICAL();
        while (budget-- && xQueueReceive(commandQueue,&item,0)==pdPASS) {
            taskENTER_CRITICAL();
            now=HAL_GetTick();
            Protect(now);
            {
                uint16_t status=WS_QueuedCommand(&g_workstation,&item.command,item.generation,now);
                Log(status==ACK_OK ? DIAG_COMMAND : DIAG_REJECT,now,
                    item.command.epoch,item.command.seq,
                    ((uint32_t)item.command.opcode<<16)|status);
                Log(DIAG_ARGUMENT,now,item.command.token,item.command.arg,item.command.opcode);
            }
            Commit(now); taskEXIT_CRITICAL();
        }
        taskENTER_CRITICAL(); progress.work=HAL_GetTick(); ++g_task_cycles[1]; taskEXIT_CRITICAL();
        g_stack_watermark[1]=(uint16_t)uxTaskGetStackHighWaterMark(NULL);
        if (g_workstation.reset_requested) { BSP_BoardFaultOutputs(); NVIC_SystemReset(); }
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(10));
    }
}
static void HealthTask(void *arg)
{
    TickType_t wake=xTaskGetTickCount(); (void)arg;
    for (;;) {
        bool healthy; uint32_t now;
#if COATING_DEBUG_INJECTION
        if (pauseHealth) { vTaskDelayUntil(&wake,pdMS_TO_TICKS(100)); continue; }
#endif
        taskENTER_CRITICAL(); now=HAL_GetTick(); Protect(now); Commit(now);
        healthy=g_workstation.health==HEALTH_ALL && !g_workstation.reset_requested;
        if (healthy) {
            ++g_workstation.mcu_heartbeat; BSP_WatchdogFeed();
            if (g_workstation.mcu_heartbeat%100U==0)
                Log(DIAG_STACK,now,g_stack_watermark[0],g_stack_watermark[1],g_stack_watermark[2]);
        }
        progress.monitor=HAL_GetTick(); ++g_task_cycles[2]; taskEXIT_CRITICAL();
        g_stack_watermark[2]=(uint16_t)uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(100));
    }
}
void App_CreateTasks(void)
{
    uint32_t now=HAL_GetTick();
    WS_Init(&g_workstation,BSP_ResetInfo()->reason,now);
    progress=(TaskProgress){now,now,now};
    Log(DIAG_BOOT,now,BSP_ResetInfo()->raw_csr,BSP_ResetInfo()->reason,BOARD_WIRING_CONFIRMED);
    Log(DIAG_CLOCK,now,SystemCoreClock,HAL_RCC_GetPCLK1Freq(),HAL_RCC_GetPCLK2Freq());
    commandQueue=xQueueCreateStatic(COMMAND_QUEUE_CAPACITY,sizeof(QueuedCommand),
                                   commandQueueStorage,&commandQueueControl);
    if (!commandQueue) Error_Handler();
    healthTask=xTaskCreateStatic(HealthTask,"Health",256,NULL,5,healthStack,&healthTCB);
    commTask=xTaskCreateStatic(CommTask,"Comm",768,NULL,4,commStack,&commTCB);
    workTask=xTaskCreateStatic(WorkTask,"Workstation",512,NULL,3,workStack,&workTCB);
    if (!healthTask || !commTask || !workTask) Error_Handler();
}
