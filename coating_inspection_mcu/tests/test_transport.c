#include "test_support.h"
#include "rtu.h"
#include "dma_cursor.h"
typedef struct { unsigned delivered,errors; size_t bytes; } Result;
static void Delivered(void *ctx,const uint8_t *data,size_t n)
{ Result *r=ctx; (void)data; ++r->delivered; r->bytes+=n; }
static void Error(void *ctx) { ++((Result *)ctx)->errors; }
int main(void)
{
    RtuFrame frame; RxRing ring; RxRecord record; Result result={0}; DmaCursor cursor={0};
    uint8_t dma[16]; uint16_t copied; unsigned i; uint32_t now=UINT32_MAX-2000;
    Rtu_Init(&frame);
    for (i=0;i<8;++i) Rtu_Byte(&frame,(uint8_t)i,now+i*96,Delivered,Error,&result);
    now+=7*96;
    Rtu_Idle(&frame,now+1750,Delivered,Error,&result); CHECK(!result.delivered);
    Rtu_Idle(&frame,now+1896,Delivered,Error,&result); CHECK(result.delivered==1 && result.bytes==8);
    Rtu_Idle(&frame,now+4000,Delivered,Error,&result); CHECK(result.delivered==1);
    now+=5000; Rtu_Byte(&frame,1,now,Delivered,Error,&result);
    Rtu_Byte(&frame,2,now+96+751,Delivered,Error,&result);
    Rtu_Idle(&frame,now+96+751+1896,Delivered,Error,&result); CHECK(result.errors==1);
    now+=6000; Rtu_Byte(&frame,3,now,Delivered,Error,&result);
    Rtu_Byte(&frame,4,now+96+1750,Delivered,Error,&result); CHECK(result.delivered==2);
    Rtu_Idle(&frame,now+96+1750+1896,Delivered,Error,&result); CHECK(result.delivered==3);
    now+=6000;
    for (i=0;i<257;++i) Rtu_Byte(&frame,(uint8_t)i,now+i*96,Delivered,Error,&result);
    Rtu_Idle(&frame,now+256*96+1896,Delivered,Error,&result); CHECK(result.errors==2);
    Rtu_Invalidate(&frame,now+30000); Rtu_Byte(&frame,1,now+30096,Delivered,Error,&result);
    Rtu_Idle(&frame,now+32000,Delivered,Error,&result); CHECK(result.errors==3);
    RxRing_Init(&ring);
    for (i=0;i<1024;++i) CHECK(RxRing_Push(&ring,(RxRecord){i,(uint8_t)i,RX_BYTE}));
    CHECK(!RxRing_Push(&ring,(RxRecord){0}));
    for (i=0;i<1024;++i) { CHECK(RxRing_Pop(&ring,&record)); CHECK(record.us==i); }
    CHECK(!RxRing_Pop(&ring,&record));
    for (i=0;i<16;++i) dma[i]=(uint8_t)i;
    CHECK(DmaCursor_Extract(&cursor,dma,16,14,2000,&ring,&copied) && copied==14);
    CHECK(DmaCursor_Extract(&cursor,dma,16,14,2050,&ring,&copied) && !copied);
    CHECK(DmaCursor_Extract(&cursor,dma,16,2,2400,&ring,&copied) && copied==4);
    for (i=0;i<18;++i) { CHECK(RxRing_Pop(&ring,&record)); CHECK(record.byte==i%16); }
    CHECK(DmaCursor_Extract(&cursor,dma,16,16,4000,&ring,&copied) && copied==14);
    RxRing_Clear(&ring); cursor.tail=0;
    for (i=0;i<1023;++i) CHECK(RxRing_Push(&ring,(RxRecord){0}));
    CHECK(!DmaCursor_Extract(&cursor,dma,16,2,6000,&ring,&copied) && cursor.tail==0 && ring.count==1023);
    RxRing_Clear(&ring); CHECK(DmaCursor_Extract(&cursor,dma,16,2,6000,&ring,&copied));
    CHECK(!DmaCursor_Extract(&cursor,dma,16,17,6000,&ring,&copied));
    puts("Transport: timestamps, gaps, timer wrap, overflow, DMA wrap and duplicate notices passed"); return 0;
}
