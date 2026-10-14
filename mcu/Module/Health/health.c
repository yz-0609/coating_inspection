#include "health.h"
uint16_t Health_Evaluate(const TaskProgress *p, uint32_t now)
{
    /* 用任务上次推进时间判断停滞：通信/健康监视 300 ms，工位 100 ms。 */
    uint16_t result=0;
    if ((uint32_t)(now-p->comm)<300U) result|=HEALTH_COMM;
    if ((uint32_t)(now-p->work)<100U) result|=HEALTH_WORK;
    if ((uint32_t)(now-p->monitor)<300U) result|=HEALTH_MONITOR;
    return result;
}
