#include "modbus.h"
#include <string.h>
uint16_t Modbus_CRC(const uint8_t *data, size_t length)
{
    uint16_t crc=0xffffU;
    size_t i;
    for (i=0;i<length;++i) {
        unsigned bit; crc^=data[i];
        for (bit=0;bit<8;++bit) crc=(uint16_t)((crc>>1)^((crc&1U)?0xa001U:0U));
    }
    return crc;
}
static void Finish(ModbusReply *r, size_t n)
{
    /* 寄存器数据用大端；RTU CRC 特例为低字节先发送。 */
    uint16_t crc=Modbus_CRC(r->data,n);
    r->data[n]=(uint8_t)crc; r->data[n+1]=(uint8_t)(crc>>8); r->length=n+2;
}
static void Exception(ModbusReply *r, uint8_t fc, uint8_t error, const ModbusService *s)
{
    r->data[0]=MODBUS_SLAVE; r->data[1]=fc|0x80U; r->data[2]=error;
    Finish(r,3); s->error(s->context);
}
void Modbus_Handle(const uint8_t *d, size_t n, const ModbusService *s, ModbusReply *r)
{
    uint16_t address, count, words[INPUT_REG_COUNT];
    size_t i;
    memset(r,0,sizeof(*r));
    /* 长度或 CRC 不合法时静默丢弃；其他从站地址（含广播）不处理。 */
    if (n<4 || n>RTU_MAX_ADU) { s->error(s->context); return; }
    if (d[0]!=MODBUS_SLAVE) return;
    if (Modbus_CRC(d,n)!=0) { s->error(s->context); return; }
    r->data[0]=MODBUS_SLAVE; r->data[1]=d[1];
    switch (d[1]) {
    case 3: case 4:
        if (n!=8) { s->error(s->context); return; }
        address=ReadBE16(d+2); count=ReadBE16(d+4);
        if (!count || count>125) { Exception(r,d[1],3,s); return; }
        if (d[1]==4) {
            if (address>=INPUT_REG_COUNT || count>INPUT_REG_COUNT-address) {
                Exception(r,d[1],2,s); return;
            }
            s->snapshot(s->context,words);
            /* 保存这次完整快照的 ACK 身份，供发送完成后确认按钮事件重新开放条件。 */
            r->full_snapshot=address==0 && count==INPUT_REG_COUNT;
            if (r->full_snapshot) {
                r->ack_epoch=JoinWords(words[20],words[21]);
                r->ack_seq=JoinWords(words[10],words[11]);
                r->ack_status=words[12]; r->event_code=words[15];
            }
            for (i=0;i<count;++i) WriteBE16(r->data+3+2*i,words[address+i]);
        } else {
            if (count>COMMAND_REG_COUNT || !s->holding(s->context,address,count,words)) {
                Exception(r,d[1],2,s); return;
            }
            for (i=0;i<count;++i) WriteBE16(r->data+3+2*i,words[i]);
        }
        r->data[2]=(uint8_t)(2*count); Finish(r,3+2*count); return;
    case 6:
        /* FC06 仅开放 0x0100 心跳寄存器，禁止分段写入业务命令。 */
        if (n!=8) { s->error(s->context); return; }
        if (ReadBE16(d+2)!=0x0100) { Exception(r,d[1],2,s); return; }
        s->heartbeat(s->context,ReadBE16(d+4)); memcpy(r->data,d,8); r->length=8; return;
    case 16: {
        /* FC10 必须一次写齐 0x0110 起的 9 个命令寄存器，防止执行半更新命令。 */
        WorkCommand command;
        if (n<9) { s->error(s->context); return; }
        address=ReadBE16(d+2); count=ReadBE16(d+4);
        if (address!=0x0110) { Exception(r,d[1],2,s); return; }
        if (count!=COMMAND_REG_COUNT || d[6]!=18 || n!=27) {
            Exception(r,d[1],3,s); return;
        }
        for (i=0;i<COMMAND_REG_COUNT;++i) words[i]=ReadBE16(d+7+2*i);
        Command_Decode(&command,words);
        /* 正常回显仅表示成功入队；业务执行结果由后续输入寄存器快照中的 ACK 返回。 */
        if (!s->enqueue(s->context,&command)) { Exception(r,d[1],6,s); return; }
        memcpy(r->data,d,6); Finish(r,6); return;
    }
    default: Exception(r,d[1],1,s); return;
    }
}
