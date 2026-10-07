#ifndef RUNTIME_F407_H
#define RUNTIME_F407_H
#include "control_f407.h"
#include "sensors_f407.h"
#define PROTOCOL_VERSION 1
#define PROTOCOL_MAX_PAYLOAD 48
typedef bool (*FrameSend)(void *,const uint8_t *,unsigned);
typedef struct {
    Control control;
    Imu imu;
    bool imu_valid, calibration_requested, motion_boundary_set;
    uint32_t motion_boundary_ms;
    uint8_t frame[PROTOCOL_MAX_PAYLOAD+5], used, expected;
    uint32_t frame_ms, telemetry_ms, dropped_tx, bad_frames;
    FrameSend send; void *context;
} Runtime;
unsigned Protocol_Frame(uint8_t,const uint8_t *,unsigned,uint8_t *);
void Runtime_Init(Runtime *,uint32_t,FrameSend,void *);
void Runtime_InvalidateMotion(Runtime *,uint32_t);
void Runtime_BeginBatch(Runtime *,uint32_t);
void Runtime_Byte(Runtime *,uint8_t,uint32_t,uint32_t);
void Runtime_Telemetry(Runtime *,uint32_t);
#endif
