#include "test_support.h"
#include "debounce.h"
#include "output_policy.h"
#include "diagnostics.h"
int main(void)
{
    Debounce d; Fixture f; TaskProgress p={100,100,100}; Diagnostics log={0}; Diagnostic event;
    WorkCommand ack; uint32_t seq; unsigned i;
    Debounce_Init(&d,false,0);
    CHECK(!Debounce_Update(&d,true,0)); CHECK(!Debounce_Update(&d,false,10));
    CHECK(!Debounce_Update(&d,true,20)); CHECK(!Debounce_Update(&d,true,49));
    CHECK(Debounce_Update(&d,true,50) && d.stable);
    CHECK(!Debounce_Update(&d,true,1000));
    CHECK(!Debounce_Update(&d,false,1000)); CHECK(!Debounce_Update(&d,false,1029));
    CHECK(Debounce_Update(&d,false,1030) && !d.stable);
    Debounce_Init(&d,false,UINT32_MAX-20);
    CHECK(!Debounce_Update(&d,true,UINT32_MAX-20)); CHECK(Debounce_Update(&d,true,9));
    FixtureReady(&f); WS_Inputs(&f.station,7,10);
    CHECK(f.station.event_code==1 && f.station.event_seq==1 && f.station.button_busy==1);
    WS_Inputs(&f.station,7,3000); CHECK(f.station.event_seq==1);
    WS_Inputs(&f.station,1,3001); CHECK(f.station.event_code==1);
    WS_Inputs(&f.station,5,3002); CHECK(f.station.event_code==1 && f.station.button_busy==2);
    f.now=3003; CHECK(Issue(&f,Cmd(1,0,3,CMD_ACK_EVENT,99))==ACK_PARAMETER);
    ack=Cmd(1,0,4,CMD_ACK_EVENT,1); CHECK(Issue(&f,ack)==ACK_OK && !f.station.event_code);
    WS_Inputs(&f.station,1,3004); WS_Inputs(&f.station,3,4204);
    CHECK(!f.station.event_code && f.station.button_busy==3); /* time alone is insufficient */
    WS_SnapshotSent(&f.station,1,3,1,0); CHECK(!f.station.rearm_response);
    WS_SnapshotSent(&f.station,1,4,1,1); CHECK(!f.station.rearm_response);
    WS_SnapshotSent(&f.station,1,4,1,0); CHECK(f.station.rearm_response);
    WS_Inputs(&f.station,1,4205); CHECK(f.station.buttons_open);
    WS_Inputs(&f.station,5,4206); CHECK(f.station.event_code==2 && f.station.event_seq==2);
    seq=f.station.event_seq; f.now=4207; ack=Cmd(1,0xabcdef,5,CMD_ACK_EVENT,seq);
    CHECK(Issue(&f,ack)==ACK_OK); WS_SnapshotSent(&f.station,1,5,1,0);
    WS_Inputs(&f.station,1,5000); CHECK(!f.station.buttons_open);
    WS_Inputs(&f.station,1,5407); CHECK(f.station.buttons_open);
    f.station.event_seq=UINT32_MAX; WS_Inputs(&f.station,3,5408);
    CHECK(f.station.reset_requested && f.station.outputs==4);
    CHECK(Health_Evaluate(&p,199)==7); CHECK(Health_Evaluate(&p,200)==5);
    p.work=399; CHECK(Health_Evaluate(&p,400)==2);
    p=(TaskProgress){UINT32_MAX-50,UINT32_MAX-50,UINT32_MAX-50}; CHECK(Health_Evaluate(&p,20)==7);
    FixtureReady(&f); WS_Heartbeat(&f.station,0xffff,UINT32_MAX-100);
    CHECK(WS_HostFresh(&f.station,100)); WS_Heartbeat(&f.station,0,100);
    CHECK(WS_HostFresh(&f.station,1599)); WS_Heartbeat(&f.station,0,1599);
    CHECK(!WS_HostFresh(&f.station,1600));
    for (i=0;i<8;++i) { CHECK(OutputPolicy_Resolve((uint16_t)i,true)==4);
        CHECK(OutputPolicy_Resolve((uint16_t)i,false)==((i==1||i==2||i==4)?i:0)); }
    for (i=0;i<DIAG_CAPACITY;++i) CHECK(Diagnostics_Push(&log,(Diagnostic){i,0,0,0,1}));
    CHECK(!Diagnostics_Push(&log,(Diagnostic){0}) && log.dropped==1);
    for (i=0;i<DIAG_CAPACITY;++i) { CHECK(Diagnostics_Pop(&log,&event)); CHECK(event.tick==i); }
    CHECK(!Diagnostics_Pop(&log,&event));
    puts("Inputs/health: debounce, rearm, exhaustion, wrap, task expiry and log pressure passed"); return 0;
}
