#include "test_support.h"
#include "reset_reason.h"
static void Flow(uint16_t result)
{
    Fixture f; WorkCommand start=Cmd(1,99,3,CMD_START,0), apply=Cmd(1,99,4,CMD_RESULT,result);
    FixtureReady(&f); CHECK(Issue(&f,start)==ACK_OK); CHECK(f.station.state==WS_INSPECTING);
    CHECK(Issue(&f,start)==ACK_OK && f.station.token==99);
    CHECK(Issue(&f,apply)==ACK_OK && f.station.state==WS_COMPLETED && f.station.outputs==result);
    WS_Inputs(&f.station,0,50); CHECK(f.station.state==WS_IDLE && !f.station.outputs && !f.station.token);
    CHECK(Issue(&f,apply)==ACK_OK && !f.station.outputs && f.station.state==WS_IDLE);
    WS_Inputs(&f.station,1,60); CHECK(f.station.state==WS_READY && !f.station.outputs);
}
int main(void)
{
    Fixture f; WorkCommand c; uint16_t words[22]; uint32_t generation;
    Flow(OUTPUT_PASS); Flow(OUTPUT_FAIL);
    FixtureReady(&f); CHECK(Issue(&f,Cmd(1,99,3,CMD_START,0))==ACK_OK);
    c=Cmd(1,99,4,CMD_RESULT,1); CHECK(Issue(&f,c)==ACK_OK);
    WS_Protect(&f.station,HEALTH_ALL,true,1500);
    f.now=1500;
    CHECK(f.station.fault==FAULT_TIMEOUT && f.station.outputs==4 && !f.station.token);
    CHECK(Issue(&f,c)==ACK_OK && f.station.outputs==4 && f.station.state==WS_FAULT);
    c.arg=2; CHECK(Issue(&f,c)==ACK_IDENTITY && f.station.cached.arg==1);
    c.arg=1; CHECK(Issue(&f,c)==ACK_OK && f.station.outputs==4);
    CHECK(Issue(&f,Cmd(0,99,8,CMD_START,0))==ACK_IDENTITY);
    CHECK(Issue(&f,Cmd(1,99,3,CMD_START,0))==ACK_IDENTITY);
    CHECK(Issue(&f,Cmd(1,99,0,CMD_START,0))==ACK_IDENTITY);
    CHECK(Issue(&f,Cmd(1,0,5,CMD_RESET,0))==ACK_HEALTH);
    f.now=1501; WS_Heartbeat(&f.station,2,1501); CHECK(Issue(&f,Cmd(1,0,5,CMD_RESET,0))==ACK_HEALTH);
    CHECK(Issue(&f,Cmd(1,0,6,CMD_RESET,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(1,0,7,CMD_RESET,0))==ACK_STATE);
    CHECK(Issue(&f,Cmd(1,22,10,CMD_START,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(1,23,11,CMD_RESULT,1))==ACK_TOKEN && f.station.token==22 && !f.station.outputs);
    CHECK(Issue(&f,Cmd(1,22,12,CMD_RESULT,3))==ACK_PARAMETER && f.station.state==WS_INSPECTING);
    WS_Inputs(&f.station,0,1502); CHECK(f.station.fault==FAULT_REMOVED && !f.station.token && f.station.outputs==4);
    WS_Inputs(&f.station,1,1503); CHECK(f.station.state==WS_FAULT && f.station.outputs==4);
    WS_Protect(&f.station,3,false,1600); CHECK(f.station.fault==FAULT_TASK);
    CHECK(Issue(&f,Cmd(2,0,1,CMD_SYNC,0))==ACK_OK && f.station.fault==FAULT_TASK);
    CHECK(!WS_HostFresh(&f.station,1600)); WS_Heartbeat(&f.station,2,1600); CHECK(!WS_HostFresh(&f.station,1600));
    WS_Heartbeat(&f.station,3,1601);
    f.now=1601;
    CHECK(Issue(&f,Cmd(2,0,2,CMD_RESET,0))==ACK_HEALTH);
    WS_Protect(&f.station,7,true,1601);
    CHECK(Issue(&f,Cmd(2,0,3,CMD_RESET,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(2,44,4,CMD_START,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(2,45,5,CMD_ABORT,0))==ACK_TOKEN && f.station.token==44);
    f.now=4000; CHECK(Issue(&f,Cmd(2,44,6,CMD_ABORT,0))==ACK_OK && f.station.fault==FAULT_ABORT);
    CHECK(Issue(&f,Cmd(2,44,7,CMD_ABORT,0))==ACK_OK && f.station.outputs==4);
    CHECK(Issue(&f,Cmd(2,0,8,99,0))==ACK_PARAMETER);
    FixtureReady(&f); generation=f.station.safety_generation;
    WS_Fault(&f.station,FAULT_RX,10);
    c=Cmd(1,0,3,CMD_RESET,0);
    CHECK(WS_QueuedCommand(&f.station,&c,generation,10)==ACK_STATE);
    CHECK(Issue(&f,c)==ACK_STATE && f.station.outputs==4);
    CHECK(Issue(&f,Cmd(1,0,4,CMD_RESET,0))==ACK_OK);
    c=Cmd(1,55,5,CMD_START,0); CHECK(Issue(&f,c)==ACK_OK);
    WS_Fault(&f.station,FAULT_TASK,20);
    CHECK(Issue(&f,Cmd(2,0,1,CMD_SYNC,0))==ACK_OK && f.station.outputs==4);
    CHECK(Issue(&f,c)==ACK_IDENTITY && f.station.outputs==4);
    FixtureReady(&f); CHECK(Issue(&f,Cmd(1,1,3,CMD_START,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(1,1,4,CMD_RESULT,1))==ACK_OK);
    f.station.safety_generation=UINT32_MAX;
    CHECK(Issue(&f,Cmd(2,0,1,CMD_SYNC,0))==ACK_HEALTH);
    CHECK(f.station.reset_requested && f.station.outputs==4 && !f.station.token);
    FixtureReady(&f);
    CHECK(Issue(&f,Cmd(2,1,900,CMD_SYNC,0))==ACK_PARAMETER && f.station.epoch==1);
    CHECK(Issue(&f,Cmd(1,55,3,CMD_START,0))==ACK_OK); /* bad future SYNC cannot poison current seq */
    WS_Snapshot(&f.station,UINT32_C(0x12345678),words);
    CHECK(words[6]==0x1234 && words[7]==0x5678 && words[19]==0x0200 && words[4]==7);
    FixtureReady(&f); CHECK(Issue(&f,Cmd(1,1,3,CMD_START,0))==ACK_OK);
    CHECK(Issue(&f,Cmd(1,1,4,CMD_RESULT,1))==ACK_OK);
    WS_Step(&f.station,0,7,false,10);
    CHECK(f.station.state==WS_FAULT && f.station.fault==FAULT_RX && f.station.outputs==4);
    FixtureReady(&f); CHECK(Issue(&f,Cmd(1,1,3,CMD_START,0))==ACK_OK);
    WS_Step(&f.station,0,7,true,1500);
    CHECK(f.station.fault==FAULT_REMOVED && f.station.outputs==4);
    FixtureReady(&f); CHECK(Issue(&f,Cmd(1,1,3,CMD_START,0))==ACK_OK);
    c=Cmd(1,1,4,CMD_RESULT,1); CHECK(Issue(&f,c)==ACK_OK);
    WS_Init(&f.station,RESET_REASON_IWDG,100);
    CHECK(f.station.state==WS_BOOT && !f.station.token && !f.station.outputs && !f.station.epoch);
    WS_Protect(&f.station,7,true,100);
    CHECK(Issue(&f,c)==ACK_IDENTITY && f.station.outputs==4);
    puts("Workstation: lifecycle, identity, cached rejection, stale queue and fault gate passed"); return 0;
}
