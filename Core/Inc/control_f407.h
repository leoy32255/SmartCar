#ifndef CONTROL_F407_H
#define CONTROL_F407_H
#include <stdint.h>
#include <stdbool.h>
enum { MODE_STOP, MODE_TRACK, MODE_RC };
enum { FAULT_HEARTBEAT=1, FAULT_SENSOR=2, FAULT_LINE=4, FAULT_AMBIGUOUS=8,
       FAULT_SCHEDULE=16, FAULT_STALL=32, FAULT_UART=64 };
typedef struct {
    uint8_t mode, mask; uint16_t fault;
    bool heartbeat_seen, sensors_valid, stop_barrier;
    uint32_t heartbeat_ms, sensor_ms, stall_since[2], last_tick;
    int16_t deviation, speed[2], target[2], pwm[2], param[6];
    int32_t integral[2], previous_error[2], previous_deviation;
} Control;
void Control_Init(Control *,uint32_t);
void Control_BeginBatch(Control *);
void Control_Stop(Control *);
void Control_Fault(Control *,uint16_t);
void Control_CheckTime(Control *,uint32_t);
void Control_Heartbeat(Control *,uint32_t);
void Control_Sensors(Control *,uint8_t,int16_t,int16_t,int16_t,bool,uint32_t);
bool Control_Start(Control *,uint8_t,uint32_t);
bool Control_Clear(Control *,uint32_t);
bool Control_Drive(Control *,int16_t,int16_t);
bool Control_Param(Control *,uint8_t,int16_t);
void Control_Tick(Control *,uint32_t);
#endif
