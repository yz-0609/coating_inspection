#include "diagnostics.h"
bool Diagnostics_Push(Diagnostics *q, Diagnostic e)
{
    if (q->count==DIAG_CAPACITY) { SaturatingIncrement(&q->dropped); return false; }
    q->entries[q->head]=e; q->head=(q->head+1U)%DIAG_CAPACITY; ++q->count; return true;
}
bool Diagnostics_Pop(Diagnostics *q, Diagnostic *e)
{
    if (!q->count) return false;
    *e=q->entries[q->tail]; q->tail=(q->tail+1U)%DIAG_CAPACITY; --q->count; return true;
}
