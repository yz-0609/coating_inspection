#include "protocol.h"
void Command_Decode(WorkCommand *c, const uint16_t w[COMMAND_REG_COUNT])
{
    c->epoch=JoinWords(w[0],w[1]); c->token=JoinWords(w[2],w[3]);
    c->seq=JoinWords(w[4],w[5]); c->opcode=w[6]; c->arg=JoinWords(w[7],w[8]);
}
void Command_Encode(const WorkCommand *c, uint16_t w[COMMAND_REG_COUNT])
{
    w[0]=(uint16_t)(c->epoch>>16); w[1]=(uint16_t)c->epoch;
    w[2]=(uint16_t)(c->token>>16); w[3]=(uint16_t)c->token;
    w[4]=(uint16_t)(c->seq>>16); w[5]=(uint16_t)c->seq; w[6]=c->opcode;
    w[7]=(uint16_t)(c->arg>>16); w[8]=(uint16_t)c->arg;
}
bool Command_Equal(const WorkCommand *a, const WorkCommand *b)
{
    return a->epoch==b->epoch && a->token==b->token && a->seq==b->seq &&
           a->opcode==b->opcode && a->arg==b->arg;
}
