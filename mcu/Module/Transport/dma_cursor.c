#include "dma_cursor.h"
bool DmaCursor_Extract(DmaCursor *c, const uint8_t *dma, uint16_t capacity,
                       uint16_t head, uint32_t now, RxRing *ring, uint16_t *copied)
{
    uint16_t count, i; uint32_t stamp;
    *copied=0;
    if (!capacity || head>capacity || c->tail>=capacity) return false;
    head%=capacity; count=(uint16_t)((head+capacity-c->tail)%capacity);
    /* 先检查整批容量，不允许只复制部分后返回错误而留下不完整数据。 */
    if (count>RX_RING_CAPACITY-ring->count) return false;
    /* 以本次采集时刻为最后一个字符，按字符时长倒推本批时间戳。
     * 这是批量采集的估计值，可靠性依赖板级接收监视未超时。 */
    stamp=now-(uint32_t)(count ? count-1U : 0U)*RTU_CHAR_US;
    for (i=0;i<count;++i) {
        (void)RxRing_Push(ring,(RxRecord){stamp,dma[c->tail],RX_BYTE});
        c->tail=(c->tail+1U)%capacity; stamp+=RTU_CHAR_US;
    }
    *copied=count; return true;
}
