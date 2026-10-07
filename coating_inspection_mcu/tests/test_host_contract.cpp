#include "test_support.h"
#include "inspection/protocol.hpp"
#include <array>
#include <iostream>
static inspection::Snapshot Read(Fixture& f)
{
    std::array<std::uint16_t,22> words{};
    WS_Snapshot(&f.station,f.now,words.data());
    return inspection::decode(words);
}
static void Send(Fixture& f,const inspection::Command& c)
{
    auto words=inspection::encode(c); WorkCommand decoded{};
    Command_Decode(&decoded,words.data());
    uint8_t bytes[27]; ModbusReply r{};
    auto n=CommandFrame(bytes,decoded);
    Modbus_Handle(bytes,n,&f.service,&r); CHECK(r.length==8);
    FixtureDrain(&f);
}
static void Run(unsigned points,unsigned result)
{
    Fixture f; FixtureInit(&f);
    inspection::Command c{0x12345678,0,1,0,inspection::Opcode::SyncSafe};
    Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
    WS_Heartbeat(&f.station,1,0); WS_Inputs(&f.station,1,0);
    c.sequence=2; c.opcode=inspection::Opcode::ResetFault;
    Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
    c.sequence=3; c.token=0x89abcdef; c.opcode=inspection::Opcode::Start;
    Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
    for (unsigned point=0;point<points;++point) {
        f.now+=1400; WS_Heartbeat(&f.station,static_cast<uint16_t>(point+2),f.now);
        WS_Inputs(&f.station,3,f.now); CHECK(f.station.event_code==1);
        c.sequence++; c.opcode=inspection::Opcode::AckInput; c.argument=f.station.event_seq;
        Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
        auto snapshot=Read(f);
        WS_SnapshotSent(&f.station,snapshot.ack_epoch,snapshot.ack_sequence,snapshot.ack_status,snapshot.event_code);
        WS_Inputs(&f.station,1,f.now);
        WS_Inputs(&f.station,1,f.now+BUTTON_HOLD_MS);
    }
    c.sequence++; c.opcode=inspection::Opcode::ApplyResult; c.argument=result;
    Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
    WS_Fault(&f.station,FAULT_RX,f.now);
    Send(f,c); CHECK(!inspection::command_satisfied(c,Read(f))); CHECK(f.station.outputs==4);
    c.sequence++; c.opcode=inspection::Opcode::Abort; c.argument=0;
    Send(f,c); CHECK(inspection::command_satisfied(c,Read(f)));
}
int main()
{
    Run(1,1); Run(1,2); Run(5,1); Run(5,2);
    std::cout<<"Unmodified upstream C++ encode/decode/command_satisfied: 1/5-point PASS/FAIL, ACK, ABORT and stale-result safety passed\n";
}
