#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H
#include <stdbool.h>
#include <stdint.h>
/* Caller holds the shared application critical section. Returns applied logical flags. */
uint16_t OutputManager_Commit(uint16_t request, bool gate);
#endif
