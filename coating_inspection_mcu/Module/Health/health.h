#ifndef HEALTH_H
#define HEALTH_H
#include <stdint.h>
enum { HEALTH_COMM=1, HEALTH_WORK=2, HEALTH_MONITOR=4, HEALTH_ALL=7 };
typedef struct { uint32_t comm, work, monitor; } TaskProgress;
uint16_t Health_Evaluate(const TaskProgress *p, uint32_t now);
#endif
