#ifndef RTU_STREAM_H
#define RTU_STREAM_H
#include "protocol.h"
typedef struct {
    uint8_t data[RTU_MAX_ADU];
    uint16_t length;
    uint32_t last_byte_us;
    bool active, invalid;
} RtuFrame;
typedef void (*RtuDeliver)(void *context, const uint8_t *, size_t);
typedef void (*RtuError)(void *context);
void Rtu_Init(RtuFrame *frame);
void Rtu_Byte(RtuFrame *frame, uint8_t byte, uint32_t timestamp,
              RtuDeliver deliver, RtuError error, void *context);
void Rtu_Idle(RtuFrame *frame, uint32_t now, RtuDeliver deliver,
              RtuError error, void *context);
void Rtu_Invalidate(RtuFrame *frame, uint32_t now);

/* 中断生产、任务消费；调用方须统一排除相关中断，环本身不提供并发锁。
 * 每条记录保留采集时赋予的字符完成时间戳；静默和错误通过独立记录传递。 */
#define RX_RING_CAPACITY 1024U
enum { RX_BYTE, RX_SILENCE, RX_ERROR };
typedef struct { uint32_t us; uint8_t byte, kind; } RxRecord;
typedef struct { RxRecord records[RX_RING_CAPACITY]; uint16_t head, tail, count; } RxRing;
void RxRing_Init(RxRing *ring);
bool RxRing_Push(RxRing *ring, RxRecord record);
bool RxRing_Pop(RxRing *ring, RxRecord *record);
void RxRing_Clear(RxRing *ring);
#endif
