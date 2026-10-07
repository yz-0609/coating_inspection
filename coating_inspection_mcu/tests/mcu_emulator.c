#include "test_support.h"
#include "debounce.h"
#include <ctype.h>
static Fixture fixture;
static Debounce inputs[3];
static uint16_t raw,health=7;
static bool rx=true;
static void Step(void)
{
    unsigned i; uint16_t flags=0;
    for (i=0;i<3;++i) {
        Debounce_Update(inputs+i,(raw & (1U<<i))!=0,fixture.now);
        if (inputs[i].stable) flags|=(uint16_t)(1U<<i);
    }
    WS_Step(&fixture.station,flags,health,rx,fixture.now);
}
static void Status(void)
{
    Workstation *s=&fixture.station;
    printf("{\"tick\":%lu,\"state\":%u,\"inputs\":%u,\"outputs\":%u,\"fault\":%u,\"token\":%lu,\"ack\":%u,\"epoch\":%lu,\"seq\":%lu,\"event_seq\":%lu,\"event_code\":%u}\n",
        (unsigned long)fixture.now,s->state,s->inputs,s->outputs,s->fault,(unsigned long)s->token,
        s->ack_status,(unsigned long)s->ack_epoch,(unsigned long)s->ack_seq,
        (unsigned long)s->event_seq,s->event_code);
}
int main(void)
{
    char line[700]; unsigned i;
    FixtureInit(&fixture); for (i=0;i<3;++i) Debounce_Init(inputs+i,false,0);
    while (fgets(line,sizeof(line),stdin)) {
        unsigned long value;
        if (sscanf(line,"T %lu",&value)==1) {
            uint32_t target=(uint32_t)value;
            if ((uint32_t)(target-fixture.now)>10000000U) { puts("ERROR tick jump"); continue; }
            while (fixture.now!=target) {
                uint32_t delta=target-fixture.now; fixture.now+=delta<10?delta:10; Step();
            }
            Status();
        } else if (sscanf(line,"I %lu",&value)==1 && value<=7) { raw=(uint16_t)value; Step(); Status(); }
        else if (sscanf(line,"H %lu",&value)==1 && value<=7) { health=(uint16_t)value; Step(); Status(); }
        else if (sscanf(line,"R %lu",&value)==1 && value<=1) { rx=value!=0; Step(); Status(); }
        else if (line[0]=='F' && line[1]==' ') {
            uint8_t data[RTU_MAX_ADU]; size_t length=0; ModbusReply reply;
            char *cursor=line+2; bool valid=true;
            while (*cursor && *cursor!='\n') {
                unsigned byte;
                while (*cursor==' ') ++cursor;
                if (*cursor=='\n' || !*cursor) break;
                if (!isxdigit((unsigned char)cursor[0]) || !isxdigit((unsigned char)cursor[1]) ||
                    length==sizeof(data) || sscanf(cursor,"%2x",&byte)!=1) { valid=false; break; }
                data[length++]=(uint8_t)byte; cursor+=2;
            }
            if (!valid) { puts("ERROR hex"); continue; }
            Modbus_Handle(data,length,&fixture.service,&reply);
            if (reply.length) for (i=0;i<reply.length;++i) printf("%02X",reply.data[i]);
            puts("");
            if (reply.full_snapshot) WS_SnapshotSent(&fixture.station,reply.ack_epoch,reply.ack_seq,reply.ack_status,reply.event_code);
            FixtureDrain(&fixture);
        } else if (line[0]=='S') Status();
        else puts("ERROR command");
        fflush(stdout);
    }
    return 0;
}
