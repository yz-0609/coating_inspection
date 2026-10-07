#ifndef WORKSTATION_H
#define WORKSTATION_H
#include "protocol.h"
typedef struct {
    uint16_t state, inputs, outputs, fault, health, mcu_heartbeat, reset_reason;
    uint16_t host_heartbeat, ack_status, event_code;
    uint32_t epoch, token, last_seq, ack_epoch, ack_seq, event_seq;
    uint32_t safety_generation;
    uint32_t heartbeat_tick, sync_tick, comm_errors, button_busy, business_rejects;
    uint32_t first_fault_tick, first_fault_token;
    uint16_t first_fault;
    bool heartbeat_valid, fault_gate, rx_healthy, cache_valid, buttons_open;
    bool rearm_pending, rearm_response, reset_requested;
    uint32_t rearm_tick, rearm_epoch, rearm_seq;
    WorkCommand cached;
    uint16_t cached_status;
} Workstation;

void WS_Init(Workstation *s, uint16_t reset_reason, uint32_t now);
void WS_Heartbeat(Workstation *s, uint16_t value, uint32_t now);
bool WS_HostFresh(const Workstation *s, uint32_t now);
void WS_Inputs(Workstation *s, uint16_t inputs, uint32_t now);
void WS_Protect(Workstation *s, uint16_t health, bool rx_healthy, uint32_t now);
void WS_Step(Workstation *s, uint16_t inputs, uint16_t health, bool rx_healthy, uint32_t now);
void WS_Fault(Workstation *s, uint16_t cause, uint32_t now);
uint16_t WS_Command(Workstation *s, const WorkCommand *c, uint32_t now);
uint16_t WS_QueuedCommand(Workstation *s, const WorkCommand *c, uint32_t generation, uint32_t now);
void WS_Snapshot(const Workstation *s, uint32_t now, uint16_t words[INPUT_REG_COUNT]);
void WS_SnapshotSent(Workstation *s, uint32_t epoch, uint32_t seq,
                     uint16_t status, uint16_t event_code);
#endif
