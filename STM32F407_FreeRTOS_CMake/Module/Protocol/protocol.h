#ifndef COATING_PROTOCOL_H
#define COATING_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FW_VERSION "1.0.0"
#define PROTOCOL_VERSION UINT16_C(0x0200)
#define MODBUS_SLAVE 1U
#define INPUT_REG_COUNT 22U
#define COMMAND_REG_COUNT 9U
#define COMMAND_QUEUE_CAPACITY 8U
#define HOST_TIMEOUT_MS 1500U
#define BUTTON_HOLD_MS 1200U
#define RTU_MAX_ADU 256U
#define RTU_T15_US 750U
#define RTU_T35_US 1750U
#define RTU_CHAR_US 96U /* ceil(11 bits / 115200 baud) */
#define RX_MONITOR_US 50U

enum { WS_BOOT, WS_IDLE, WS_READY, WS_INSPECTING, WS_COMPLETED, WS_FAULT };
enum { CMD_SYNC=1, CMD_START, CMD_RESULT, CMD_ACK_EVENT, CMD_RESET, CMD_ABORT };
enum { ACK_NONE, ACK_OK, ACK_STATE, ACK_TOKEN, ACK_IDENTITY, ACK_HEALTH, ACK_PARAMETER };
enum { FAULT_NONE, FAULT_TIMEOUT, FAULT_REMOVED, FAULT_RX, FAULT_TASK,
       FAULT_ABORT, FAULT_UNSYNC };
enum { INPUT_PRESENT=1, INPUT_ACTION=2, INPUT_RESET=4 };
enum { OUTPUT_PASS=1, OUTPUT_FAIL=2, OUTPUT_FAULT=4 };

typedef struct {
    uint32_t epoch, token, seq;
    uint16_t opcode;
    uint32_t arg;
} WorkCommand;

static inline uint16_t ReadBE16(const uint8_t *p)
{ return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static inline void WriteBE16(uint8_t *p, uint16_t value)
{ p[0]=(uint8_t)(value >> 8); p[1]=(uint8_t)value; }
static inline uint32_t JoinWords(uint16_t high, uint16_t low)
{ return ((uint32_t)high << 16) | low; }
static inline void SaturatingIncrement(uint32_t *value)
{ if (*value != UINT32_MAX) ++*value; }
static inline uint32_t Elapsed32(uint32_t now, uint32_t then)
{ return now-then; }

void Command_Decode(WorkCommand *command, const uint16_t words[COMMAND_REG_COUNT]);
void Command_Encode(const WorkCommand *command, uint16_t words[COMMAND_REG_COUNT]);
bool Command_Equal(const WorkCommand *a, const WorkCommand *b);
#endif
