#include "runtime_f407.h"
#include "host_assert.h"
#include <string.h>
static unsigned emitted;
static bool send(void *ctx,const uint8_t *p,unsigned n) { (void)ctx; assert(n>=5 && p[0]==0xaa); emitted++; return true; }
static void command(Runtime *r,uint8_t cmd,const uint8_t *p,unsigned n,uint32_t at) {
    uint8_t f[70]; unsigned len=Protocol_Frame(cmd,p,n,f);
    for(unsigned i=0;i<len;i++) Runtime_Byte(r,f[i],at,at);
}
int main(void) {
    Runtime r; Runtime_Init(&r,0,send,0);
    Control_Sensors(&r.control,4,0,0,0,true,0);
    command(&r,6,0,0,0); uint8_t mode=2; command(&r,1,&mode,1,0);
    assert(r.control.mode==MODE_RC);
    uint8_t drive[]={5,0,5,0}; command(&r,2,drive,4,0);
    Control_Tick(&r.control,10); assert(r.control.pwm[0]>0);
    mode=0; command(&r,1,&mode,1,11); mode=2; command(&r,1,&mode,1,11);
    assert(r.control.mode==MODE_STOP);
    Runtime_BeginBatch(&r,12); command(&r,1,&mode,1,12); assert(r.control.mode==MODE_RC);
    uint8_t bad[]={0xaa,0x55,6,0,7};
    for(unsigned i=0;i<sizeof bad;i++) Runtime_Byte(&r,bad[i],499,499);
    Runtime_BeginBatch(&r,500); assert(r.control.fault&FAULT_HEARTBEAT);
    Runtime_Init(&r,0,send,0);Control_Sensors(&r.control,4,0,0,0,true,0);
    uint8_t heart[8];unsigned hn=Protocol_Frame(6,0,0,heart);
    for(unsigned i=0;i<hn;i++) Runtime_Byte(&r,heart[i],0,101);
    assert(!r.control.heartbeat_seen);
    Runtime_Byte(&r,0xaa,110,110);Runtime_Byte(&r,0x55,110,110);
    command(&r,6,0,0,220);assert(r.control.heartbeat_seen);
    Runtime_Init(&r,0,send,0);Control_Sensors(&r.control,4,0,0,0,true,0);
    command(&r,6,0,0,0);mode=2;command(&r,1,&mode,1,0);
    uint8_t oversized[]={0xaa,0x55,2,255};
    for(unsigned i=0;i<sizeof oversized;i++) Runtime_Byte(&r,oversized[i],1,1);
    assert(r.bad_frames==1);command(&r,2,drive,4,2);assert(r.control.target[0]==5);
    command(&r,2,drive,3,3);assert(r.control.target[0]==5);
    Runtime_BeginBatch(&r,500);command(&r,6,0,0,500);assert(r.control.fault&FAULT_HEARTBEAT);
    Control_Sensors(&r.control,4,0,0,0,true,500);command(&r,7,0,0,500);assert(r.control.mode==MODE_STOP);
    Runtime_Init(&r,0,send,0);Control_Sensors(&r.control,4,0,0,0,true,0);
    command(&r,6,0,0,0);
    mode=0; command(&r,1,&mode,1,10);
    uint8_t start[8];mode=1;unsigned sn=Protocol_Frame(1,&mode,1,start);
    for(unsigned i=0;i<3;i++) Runtime_Byte(&r,start[i],9,10);
    Runtime_BeginBatch(&r,11);
    for(unsigned i=3;i<sn;i++) Runtime_Byte(&r,start[i],9,11);
    assert(r.control.mode==MODE_STOP);
    Runtime_BeginBatch(&r,12);command(&r,1,&mode,1,12);assert(r.control.mode==MODE_TRACK);
    assert(emitted>0);
    return 0;
}
