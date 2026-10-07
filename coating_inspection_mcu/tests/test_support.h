#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
#ifdef __cplusplus
extern "C" {
#endif
#include "workstation.h"
#include "modbus.h"
#include "health.h"
#ifdef __cplusplus
}
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); exit(1); } } while (0)
typedef struct {
    Workstation station;
    uint32_t now;
    WorkCommand queued[COMMAND_QUEUE_CAPACITY];
    uint32_t generation[COMMAND_QUEUE_CAPACITY];
    unsigned count;
    ModbusService service;
} Fixture;
static inline void FixtureSnapshot(void *ctx,uint16_t *words)
{ Fixture *f=(Fixture *)ctx; WS_Snapshot(&f->station,f->now,words); }
static inline bool FixtureHolding(void *ctx,uint16_t address,uint16_t count,uint16_t *words)
{
    Fixture *f=(Fixture *)ctx; uint16_t all[9];
    if (address==0x0100 && count==1) { words[0]=f->station.host_heartbeat; return true; }
    if (address<0x0110 || address>0x0118 || count>0x0119-address) return false;
    Command_Encode(&f->station.cached,all); memcpy(words,all+(address-0x0110),count*sizeof(*words)); return true;
}
static inline void FixtureHeartbeat(void *ctx,uint16_t value)
{ Fixture *f=(Fixture *)ctx; WS_Heartbeat(&f->station,value,f->now); }
static inline bool FixtureEnqueue(void *ctx,const WorkCommand *c)
{
    Fixture *f=(Fixture *)ctx;
    if (f->count==COMMAND_QUEUE_CAPACITY) return false;
    f->queued[f->count]=*c; f->generation[f->count]=f->station.safety_generation; ++f->count; return true;
}
static inline void FixtureError(void *ctx)
{ SaturatingIncrement(&((Fixture *)ctx)->station.comm_errors); }
static inline void FixtureInit(Fixture *f)
{
    memset(f,0,sizeof(*f)); WS_Init(&f->station,0x0043,0);
    WS_Protect(&f->station,HEALTH_ALL,true,0);
    {
        ModbusService service={f,FixtureSnapshot,FixtureHolding,FixtureHeartbeat,FixtureEnqueue,FixtureError};
        f->service=service;
    }
}
static inline void FixtureDrain(Fixture *f)
{
    unsigned i;
    for (i=0;i<f->count;++i)
        WS_QueuedCommand(&f->station,f->queued+i,f->generation[i],f->now);
    f->count=0;
}
static inline WorkCommand Cmd(uint32_t epoch,uint32_t token,uint32_t seq,uint16_t opcode,uint32_t arg)
{ WorkCommand c={epoch,token,seq,opcode,arg}; return c; }
static inline uint16_t Issue(Fixture *f,WorkCommand c)
{ return WS_Command(&f->station,&c,f->now); }
static inline void FixtureReady(Fixture *f)
{
    FixtureInit(f); CHECK(Issue(f,Cmd(1,0,1,CMD_SYNC,0))==ACK_OK);
    WS_Heartbeat(&f->station,1,0); WS_Inputs(&f->station,INPUT_PRESENT,0);
    CHECK(Issue(f,Cmd(1,0,2,CMD_RESET,0))==ACK_OK); CHECK(f->station.state==WS_READY);
}
static inline size_t AddCRC(uint8_t *data,size_t n)
{ uint16_t crc=Modbus_CRC(data,n); data[n]=(uint8_t)crc; data[n+1]=(uint8_t)(crc>>8); return n+2; }
static inline size_t CommandFrame(uint8_t *data,WorkCommand c)
{
    uint16_t words[9]; unsigned i;
    data[0]=1; data[1]=16; WriteBE16(data+2,0x0110); WriteBE16(data+4,9); data[6]=18;
    Command_Encode(&c,words); for (i=0;i<9;++i) WriteBE16(data+7+2*i,words[i]); return AddCRC(data,25);
}
#endif
