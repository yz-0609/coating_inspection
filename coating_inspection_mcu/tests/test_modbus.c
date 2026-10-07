#include "test_support.h"
static size_t Hex(const char *text,uint8_t *data)
{
    size_t n=0; unsigned byte;
    while (*text) { CHECK(sscanf(text,"%2x",&byte)==1); data[n++]=(uint8_t)byte;
        text+=2; if (*text==' ') ++text; }
    return n;
}
static void Request(Fixture *f,uint8_t fc,uint16_t address,uint16_t count,ModbusReply *r)
{
    uint8_t data[8]={1,0}; data[1]=fc; WriteBE16(data+2,address); WriteBE16(data+4,count);
    Modbus_Handle(data,AddCRC(data,6),&f->service,r);
}
int main(void)
{
    static const char *golden[]={
        "01 04 00 00 00 16 71 C4", "01 06 01 00 00 01 49 F6",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 00 00 00 00 01 00 01 00 00 00 00 BE FD",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 00 00 00 00 02 00 05 00 00 00 00 7C 3D",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 63 00 00 00 03 00 02 00 00 00 00 28 78",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 63 00 00 00 04 00 03 00 00 00 01 A2 B8",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 63 00 00 00 04 00 03 00 00 00 02 E2 B9",
        "01 10 01 10 00 09 12 00 00 00 01 00 00 00 63 00 00 00 05 00 06 00 00 00 00 BF B8"};
    Fixture f; ModbusReply r; uint8_t data[300]; size_t n; unsigned i;
    FixtureInit(&f);
    for (i=0;i<sizeof(golden)/sizeof(golden[0]);++i) {
        n=Hex(golden[i],data); CHECK(Modbus_CRC(data,n)==0);
        Modbus_Handle(data,n,&f.service,&r); CHECK(r.length>0); CHECK(Modbus_CRC(r.data,r.length)==0);
    }
    n=Hex("01 03 00 00 00 0A",data); CHECK(Modbus_CRC(data,n)==0xcdc5);
    FixtureReady(&f);
    Request(&f,4,0,22,&r); CHECK(r.length==49 && r.full_snapshot && r.data[2]==44);
    CHECK(ReadBE16(r.data+3)==WS_READY && ReadBE16(r.data+3+19*2)==0x0200);
    CHECK(ReadBE16(r.data+3+16*2)==0x0043);
    Request(&f,4,1,1,&r); CHECK(r.length==7 && !r.full_snapshot);
    Request(&f,4,0,0,&r); CHECK(r.data[1]==0x84 && r.data[2]==3);
    Request(&f,4,0,126,&r); CHECK(r.data[2]==3);
    Request(&f,4,21,2,&r); CHECK(r.data[2]==2);
    Request(&f,4,0xffff,1,&r); CHECK(r.data[2]==2);
    Request(&f,3,0x0100,1,&r); CHECK(ReadBE16(r.data+3)==1);
    Request(&f,3,0x0100,2,&r); CHECK(r.data[2]==2);
    Request(&f,3,0x0110,9,&r); CHECK(r.length==23);
    for (i=0;i<9;++i) { Request(&f,3,(uint16_t)(0x0110+i),(uint16_t)(9-i),&r); CHECK(r.length==3+2*(9-i)+2); }
    Request(&f,3,0x0118,2,&r); CHECK(r.data[2]==2);
    Request(&f,6,0x0101,2,&r); CHECK(r.data[1]==0x86 && r.data[2]==2);
    Request(&f,6,0x0100,0xffff,&r); CHECK(r.length==8 && f.station.host_heartbeat==0xffff);
    Request(&f,6,0x0100,0,&r); CHECK(f.station.host_heartbeat==0);
    Request(&f,0x45,0,1,&r); CHECK(r.data[1]==0xc5 && r.data[2]==1);
    n=CommandFrame(data,Cmd(1,0x12345678,10,CMD_START,0));
    Modbus_Handle(data,n,&f.service,&r); CHECK(r.length==8 && f.count==1);
    CHECK(f.station.state==WS_READY); FixtureDrain(&f); CHECK(f.station.token==0x12345678);
    Request(&f,3,0x0112,2,&r); CHECK(ReadBE16(r.data+3)==0x1234 && ReadBE16(r.data+5)==0x5678);
    for (i=0;i<8;++i) { Modbus_Handle(data,n,&f.service,&r); CHECK(r.length==8); }
    Modbus_Handle(data,n,&f.service,&r); CHECK(r.data[1]==0x90 && r.data[2]==6 && f.count==8);
    FixtureDrain(&f);
    data[6]=17; AddCRC(data,25); Modbus_Handle(data,n,&f.service,&r); CHECK(r.data[2]==3 && !f.count);
    data[6]=18; WriteBE16(data+4,8); AddCRC(data,25); Modbus_Handle(data,n,&f.service,&r); CHECK(r.data[2]==3);
    WriteBE16(data+2,0x0111); AddCRC(data,25); Modbus_Handle(data,n,&f.service,&r); CHECK(r.data[2]==2);
    n=CommandFrame(data,Cmd(1,0,11,CMD_ABORT,0)); data[n-1]^=1;
    Modbus_Handle(data,n,&f.service,&r); CHECK(!r.length && !f.count);
    n=CommandFrame(data,Cmd(1,0,11,CMD_ABORT,0)); data[0]=0; AddCRC(data,25);
    Modbus_Handle(data,n,&f.service,&r); CHECK(!r.length && !f.count);
    data[0]=2; AddCRC(data,25); Modbus_Handle(data,n,&f.service,&r); CHECK(!r.length);
    for (i=0;i<4;++i) { Modbus_Handle(data,i,&f.service,&r); CHECK(!r.length); }
    memset(data,0,sizeof(data)); data[0]=1; Modbus_Handle(data,257,&f.service,&r); CHECK(!r.length);
    f.station.comm_errors=UINT32_MAX; Modbus_Handle(data,0,&f.service,&r); CHECK(f.station.comm_errors==UINT32_MAX);
    puts("Modbus: 8 golden vectors, all functions, ranges, queue, CRC and broadcast passed"); return 0;
}
