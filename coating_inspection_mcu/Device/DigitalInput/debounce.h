#ifndef DEBOUNCE_H
#define DEBOUNCE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool stable, candidate; uint32_t since; } Debounce;
void Debounce_Init(Debounce *input, bool initial, uint32_t now);
bool Debounce_Update(Debounce *input, bool raw, uint32_t now);
#endif
