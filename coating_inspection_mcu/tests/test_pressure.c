#include "test_support.h"
#include "rtu.h"
static uint32_t rng=UINT32_C(0x4070200);
static uint32_t Random(void)
{ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static void Invariant(const Workstation *s)
{
    CHECK(s->outputs==0 || s->outputs==1 || s->outputs==2 || s->outputs==4);
    if (s->fault_gate) CHECK(s->outputs==4);
    if (s->outputs==1 || s->outputs==2)
        CHECK(s->state==WS_COMPLETED && s->token && (s->inputs & 1) && !s->fault_gate);
    if (s->state==WS_FAULT) CHECK(!s->token && s->outputs==4);
}
int main(void)
{
    Fixture f; uint8_t bytes[300]; ModbusReply reply; unsigned iteration,i;
    FixtureReady(&f);
    for (iteration=0;iteration<100000;++iteration) {
        size_t n=Random()%301;
        for (i=0;i<n;++i) bytes[i]=(uint8_t)Random();
        if (n>=4 && n<=256 && iteration%3==0) { bytes[0]=1; AddCRC(bytes,n-2); }
        Modbus_Handle(bytes,n,&f.service,&reply);
        if (reply.length) { CHECK(reply.length<=RTU_MAX_ADU); CHECK(!Modbus_CRC(reply.data,reply.length)); }
        FixtureDrain(&f); Invariant(&f.station);
    }
    /* Both serializations of HealthTask racing with APPLY_RESULT. */
    for (iteration=0;iteration<1000;++iteration) {
        WorkCommand result=Cmd(1,99,4,CMD_RESULT,1);
        FixtureReady(&f); CHECK(Issue(&f,Cmd(1,99,3,CMD_START,0))==ACK_OK);
        if (iteration & 1) {
            WS_Protect(&f.station,3,true,0); CHECK(Issue(&f,result)==ACK_STATE);
        } else {
            CHECK(Issue(&f,result)==ACK_OK); WS_Protect(&f.station,3,true,0);
            CHECK(Issue(&f,result)==ACK_OK);
        }
        CHECK(f.station.outputs==4); Invariant(&f.station);
    }
    /* 10000 complete virtual cycles with different identities and fault causes. */
    FixtureInit(&f);
    for (iteration=1;iteration<=10000;++iteration) {
        WorkCommand result; uint16_t hb=(uint16_t)iteration;
        f.now+=2000; WS_Protect(&f.station,7,true,f.now);
        CHECK(Issue(&f,Cmd(iteration,0,1,CMD_SYNC,0))==ACK_OK);
        WS_Heartbeat(&f.station,hb,f.now); WS_Inputs(&f.station,1,f.now);
        CHECK(Issue(&f,Cmd(iteration,0,2,CMD_RESET,0))==ACK_OK);
        CHECK(Issue(&f,Cmd(iteration,iteration,3,CMD_START,0))==ACK_OK);
        result=Cmd(iteration,iteration,4,CMD_RESULT,(iteration&1)?1:2);
        CHECK(Issue(&f,result)==ACK_OK); Invariant(&f.station);
        switch (iteration%4) {
        case 0: WS_Inputs(&f.station,0,f.now+10); break;
        case 1: WS_Protect(&f.station,7,false,f.now+10); break;
        case 2: WS_Protect(&f.station,7,true,f.now+1500); break;
        default: CHECK(Issue(&f,Cmd(iteration,iteration,5,CMD_ABORT,0))==ACK_OK); break;
        }
        if (iteration%4!=3) {
            CHECK(Issue(&f,result)==ACK_OK); CHECK(f.station.outputs!=1 && f.station.outputs!=2);
        }
        Invariant(&f.station);
    }
    puts("Pressure: 100000 random ADUs, 1000 protection interleavings, 10000 virtual cycles passed"); return 0;
}
