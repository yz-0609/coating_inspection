#include "rtu.h"
#include <string.h>
void Rtu_Init(RtuFrame *f) { memset(f,0,sizeof(*f)); }
static void Flush(RtuFrame *f, RtuDeliver deliver, RtuError error, void *context)
{
    if (f->active) {
        if (f->invalid || !f->length) error(context);
        else deliver(context,f->data,f->length);
    }
    f->active=false; f->invalid=false; f->length=0;
}
void Rtu_Byte(RtuFrame *f, uint8_t b, uint32_t now,
              RtuDeliver deliver, RtuError error, void *context)
{
    if (f->active) {
        uint32_t gap=Elapsed32(now,f->last_byte_us);
        gap=gap>RTU_CHAR_US ? gap-RTU_CHAR_US : 0;
        if (gap>=RTU_T35_US) Flush(f,deliver,error,context);
        else if (gap>RTU_T15_US) f->invalid=true;
    }
    f->active=true; f->last_byte_us=now;
    if (f->length<RTU_MAX_ADU) f->data[f->length++]=b;
    else f->invalid=true;
}
void Rtu_Idle(RtuFrame *f, uint32_t now, RtuDeliver deliver, RtuError error, void *context)
{
    /* Wait out a possible character already on the wire but not yet DMA-visible. */
    if (f->active && Elapsed32(now,f->last_byte_us)>=RTU_T35_US+RTU_CHAR_US+RX_MONITOR_US)
        Flush(f,deliver,error,context);
}
void Rtu_Invalidate(RtuFrame *f, uint32_t now)
{ f->invalid=true; f->active=true; f->last_byte_us=now; }
void RxRing_Init(RxRing *r) { memset(r,0,sizeof(*r)); }
bool RxRing_Push(RxRing *r, RxRecord record)
{
    if (r->count==RX_RING_CAPACITY) return false;
    r->records[r->head]=record; r->head=(r->head+1U)%RX_RING_CAPACITY; ++r->count;
    return true;
}
bool RxRing_Pop(RxRing *r, RxRecord *record)
{
    if (!r->count) return false;
    *record=r->records[r->tail]; r->tail=(r->tail+1U)%RX_RING_CAPACITY; --r->count;
    return true;
}
void RxRing_Clear(RxRing *r) { r->head=0; r->tail=0; r->count=0; }
