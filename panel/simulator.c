#include "runtime_f407.h"
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
static Runtime runtime;
static uint8_t outgoing[8192];
static unsigned length;
static uint32_t clock_ms;
static uint8_t line=4;
static bool valid=true;
static bool capture(void *ctx,const uint8_t *data,unsigned n) {
    (void)ctx;
    if(length+n>sizeof outgoing) return false;
    memcpy(outgoing+length,data,n);length+=n;return true;
}
EXPORT void sim_init(void) {
    length=0;clock_ms=0;line=4;valid=true;Runtime_Init(&runtime,0,capture,0);
    Control_Sensors(&runtime.control,4,0,0,0,true,0);
    runtime.imu_valid=true;runtime.imu.accel[2]=16384;
}
EXPORT void sim_advance(uint32_t now) {
    while(now-clock_ms>=5U) {
        clock_ms+=5;
        Control_Sensors(&runtime.control,line,0,runtime.control.target[0],runtime.control.target[1],valid,clock_ms);
        Control_Tick(&runtime.control,clock_ms);
        if(runtime.calibration_requested) {runtime.calibration_requested=false;runtime.imu.calibrated=true;}
        Runtime_Telemetry(&runtime,clock_ms);
    }
}
EXPORT void sim_write(const uint8_t *data,unsigned n) {
    Runtime_BeginBatch(&runtime,clock_ms);
    for(unsigned i=0;i<n;i++) Runtime_Byte(&runtime,data[i],clock_ms,clock_ms);
}
EXPORT unsigned sim_read(uint8_t *data,unsigned capacity) {
    unsigned n=length<capacity?length:capacity;memcpy(data,outgoing,n);
    memmove(outgoing,outgoing+n,length-n);length-=n;return n;
}
EXPORT void sim_fault(unsigned value) {line=value==1?0:value==3?31:4;valid=value!=2;}
