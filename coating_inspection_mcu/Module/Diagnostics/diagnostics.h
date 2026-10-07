#ifndef COATING_DIAGNOSTICS_H
#define COATING_DIAGNOSTICS_H
#include "protocol.h"
#define DIAG_CAPACITY 32U
enum { DIAG_BOOT=1, DIAG_STATE, DIAG_COMMAND, DIAG_REJECT, DIAG_FIRST_FAULT,
       DIAG_OUTPUT, DIAG_RX, DIAG_STACK, DIAG_CLOCK, DIAG_ARGUMENT, DIAG_EVENT };
typedef struct { uint32_t tick, a, b, c; uint16_t code; } Diagnostic;
typedef struct { Diagnostic entries[DIAG_CAPACITY]; uint16_t head,tail,count; uint32_t dropped; } Diagnostics;
bool Diagnostics_Push(Diagnostics *queue, Diagnostic event);
bool Diagnostics_Pop(Diagnostics *queue, Diagnostic *event);
#endif
