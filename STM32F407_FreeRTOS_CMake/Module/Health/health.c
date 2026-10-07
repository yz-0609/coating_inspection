#include "health.h"
uint16_t Health_Evaluate(const TaskProgress *p, uint32_t now)
{
    uint16_t result=0;
    if ((uint32_t)(now-p->comm)<300U) result|=HEALTH_COMM;
    if ((uint32_t)(now-p->work)<100U) result|=HEALTH_WORK;
    if ((uint32_t)(now-p->monitor)<300U) result|=HEALTH_MONITOR;
    return result;
}
