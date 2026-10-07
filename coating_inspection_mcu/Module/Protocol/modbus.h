#ifndef COATING_MODBUS_H
#define COATING_MODBUS_H
#include "protocol.h"
typedef struct {
    void *context;
    void (*snapshot)(void *, uint16_t[INPUT_REG_COUNT]);
    bool (*holding)(void *, uint16_t, uint16_t, uint16_t *);
    void (*heartbeat)(void *, uint16_t);
    bool (*enqueue)(void *, const WorkCommand *);
    void (*error)(void *);
} ModbusService;
typedef struct {
    uint8_t data[RTU_MAX_ADU];
    size_t length;
    bool full_snapshot;
    uint32_t ack_epoch, ack_seq;
    uint16_t ack_status, event_code;
} ModbusReply;
uint16_t Modbus_CRC(const uint8_t *data, size_t length);
void Modbus_Handle(const uint8_t *data, size_t length,
                   const ModbusService *service, ModbusReply *reply);
#endif
