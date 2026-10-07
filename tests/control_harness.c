#include "control_f407.h"
#include "host_assert.h"
int main(void) {
    Control c; Control_Init(&c,0);
    Control_Sensors(&c,4,0,0,0,true,0);
    assert(!Control_Start(&c,MODE_RC,0));
    Control_Heartbeat(&c,0);
    assert(Control_Start(&c,MODE_RC,0));
    assert(Control_Drive(&c,5,5));
    Control_Tick(&c,10);
    assert(c.pwm[0]>0 && c.pwm[1]>0);
    Control_CheckTime(&c,499); assert(c.mode==MODE_RC);
    Control_Heartbeat(&c,500); assert(c.mode==MODE_STOP && (c.fault & FAULT_HEARTBEAT));
    Control_Sensors(&c,4,0,0,0,true,500);
    assert(Control_Clear(&c,500)); assert(c.mode==MODE_STOP && c.pwm[0]==0);
    Control_BeginBatch(&c); assert(Control_Start(&c,MODE_RC,500));
    Control_Stop(&c); assert(!Control_Start(&c,MODE_TRACK,500));
    Control_BeginBatch(&c); assert(Control_Start(&c,MODE_TRACK,500));
    Control_Sensors(&c,0,0,0,0,true,505); Control_Tick(&c,505);
    assert(c.mode==MODE_STOP && (c.fault&FAULT_LINE));
    for(unsigned mode_id=1;mode_id<=2;mode_id++) {
        for(unsigned offset=499;offset<=501;offset++) {
            Control_Init(&c,0xfffffff0U);
            Control_Sensors(&c,4,0,0,0,true,0xfffffff0U); Control_Heartbeat(&c,0xfffffff0U);
            assert(Control_Start(&c,mode_id,0xfffffff0U));
            Control_CheckTime(&c,0xfffffff0U+offset);
            assert((c.mode==MODE_STOP)==(offset>=500));
        }
    }
    Control_Init(&c,0); Control_Sensors(&c,4,0,0,0,true,0); Control_Heartbeat(&c,0);
    assert(Control_Start(&c,MODE_RC,0)); assert(Control_Drive(&c,10,10));
    for(unsigned t=10;t<=500;t+=10) {
        Control_Heartbeat(&c,t);Control_Sensors(&c,4,0,0,0,true,t);Control_Tick(&c,t);
    }
    assert(c.fault&FAULT_STALL);assert(c.pwm[0]==0 && c.pwm[1]==0);
    Control_Init(&c,0); Control_Sensors(&c,4,0,0,0,true,0);Control_Heartbeat(&c,0);
    assert(Control_Start(&c,MODE_RC,0));Control_Sensors(&c,4,0,0,0,true,21);Control_Tick(&c,21);
    assert(c.fault&FAULT_SCHEDULE);
    Control_Init(&c,0); Control_Sensors(&c,16,200,0,0,true,0);Control_Heartbeat(&c,0);
    assert(Control_Start(&c,MODE_TRACK,0));Control_Tick(&c,10);
    assert(c.target[0]>c.target[1] && c.pwm[0]<=300);
    Control_Sensors(&c,31,0,0,0,true,15);Control_Tick(&c,15);
    assert(c.fault&FAULT_AMBIGUOUS);
    assert(!Control_Param(&c,0,21));assert(!Control_Param(&c,6,0));assert(Control_Param(&c,0,10));
    Control_Sensors(&c,4,0,0,0,false,20);assert(!Control_Clear(&c,20));
    Control_Sensors(&c,4,0,0,0,true,20);assert(Control_Clear(&c,20));assert(c.mode==MODE_STOP);
    return 0;
}
