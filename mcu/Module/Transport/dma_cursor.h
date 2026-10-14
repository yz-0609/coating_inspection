#ifndef DMA_CURSOR_H
#define DMA_CURSOR_H
#include "rtu.h"
typedef struct { uint16_t tail; } DmaCursor;
bool DmaCursor_Extract(DmaCursor *cursor, const uint8_t *dma, uint16_t capacity,
                       uint16_t head, uint32_t now, RxRing *ring, uint16_t *copied);
#endif
