#include "runtime_f407.h"
#include "app_config_f407.h"
static int16_t le16(const uint8_t *p) { unsigned v=p[0]+256U*p[1]; return (int16_t)(v<32768?(int)v:(int)v-65536); }
static void put16(uint8_t *p,int32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)((uint32_t)v>>8); }
unsigned Protocol_Frame(uint8_t cmd,const uint8_t *payload,unsigned n,uint8_t *out) {
    if(n>PROTOCOL_MAX_PAYLOAD) return 0;
    out[0]=0xaa;out[1]=0x55;out[2]=cmd;out[3]=(uint8_t)n;
    uint8_t sum=cmd+(uint8_t)n;
    for(unsigned i=0;i<n;i++) { out[4+i]=payload[i];sum+=payload[i]; }
    out[4+n]=sum;return n+5;
}
static void emit(Runtime *r,uint8_t cmd,const uint8_t *p,unsigned n) {
    uint8_t f[PROTOCOL_MAX_PAYLOAD+5]; unsigned size=Protocol_Frame(cmd,p,n,f);
    if(!r->send(r->context,f,size)) ++r->dropped_tx;
}
void Runtime_Init(Runtime *r,uint32_t now,FrameSend send,void *ctx) {
    *r=(Runtime){.send=send,.context=ctx,.telemetry_ms=now}; Control_Init(&r->control,now);
}
void Runtime_InvalidateMotion(Runtime *r,uint32_t now) {
    r->motion_boundary_set=true;r->motion_boundary_ms=now;
}
void Runtime_BeginBatch(Runtime *r,uint32_t now) {
    uint8_t previous=r->control.mode;
    Control_BeginBatch(&r->control); Control_CheckTime(&r->control,now);
    if(previous!=MODE_STOP && r->control.mode==MODE_STOP) Runtime_InvalidateMotion(r,now);
}
static void dispatch(Runtime *r,uint32_t stamp,uint32_t now) {
    uint8_t cmd=r->frame[2], n=r->frame[3], *p=r->frame+4;
    bool ok=false;
    Control *c=&r->control;
    Control_CheckTime(c,now);
    /* STOP is always accepted, even when an old frame was delayed in RX. */
    if(cmd==1 && n==1 && p[0]==0) { Control_Stop(c); Runtime_InvalidateMotion(r,now); ok=true; }
    else if(now-stamp<=100U) {
        switch(cmd) {
        case 1: if(!APP_DIAGNOSTIC && n==1 && (!r->motion_boundary_set || (stamp-r->motion_boundary_ms>0U && stamp-r->motion_boundary_ms<0x80000000U)) && !r->imu.calibrating && !r->calibration_requested) ok=Control_Start(c,p[0],now); break;
        case 2: if(n==4) ok=Control_Drive(c,le16(p),le16(p+2)); break;
        case 3: if(n==3) ok=Control_Param(c,p[0],le16(p+1)); break;
        case 4: if(n==0 && c->mode==MODE_STOP && r->imu_valid) { r->calibration_requested=true;ok=true; } break;
        case 6:
            if(n==0) { Control_Heartbeat(c,stamp); return; }
            break;
        case 7: if(n==0) {ok=Control_Clear(c,now); if(ok) Runtime_InvalidateMotion(r,now);} break;
        case 8:
            if(n==0) {
                uint8_t params[12];
                for(unsigned i=0;i<6;i++) put16(params+i*2,c->param[i]);
                emit(r,0x84,params,12); ok=true;
            }
            break;
        default: break;
        }
    }
    uint8_t ack[]={cmd,ok?0:1};emit(r,0x82,ack,2);
}
void Runtime_Byte(Runtime *r,uint8_t byte,uint32_t stamp,uint32_t now) {
    if(r->used && stamp-r->frame_ms>100U) { r->used=0;++r->bad_frames; }
    if(!r->used) { if(byte==0xaa) {r->frame[0]=byte;r->used=1;r->frame_ms=stamp;} return; }
    if(r->used==1) {
        if(byte==0x55) {r->frame[1]=byte;r->used=2;}
        else if(byte!=0xaa) r->used=0;
        else r->frame_ms=stamp;
        return;
    }
    r->frame[r->used++]=byte;
    if(r->used==4) {
        if(byte>PROTOCOL_MAX_PAYLOAD) {r->used=0;++r->bad_frames;return;}
        r->expected=byte+5;
    }
    if(r->used>=5 && r->used==r->expected) {
        uint8_t sum=0;
        for(unsigned i=2;i<(unsigned)r->used-1;i++) sum+=r->frame[i];
        if(sum==byte) dispatch(r,r->frame_ms,now); else ++r->bad_frames;
        r->used=0;
    }
}
void Runtime_Telemetry(Runtime *r,uint32_t now) {
    if(now-r->telemetry_ms<100U) return;
    r->telemetry_ms=now;
    uint8_t p[42]={PROTOCOL_VERSION,r->control.mode};
    Control *c=&r->control;
    put16(p+2,c->fault);p[4]=c->mask;
    p[5]=(c->sensors_valid?1:0)|(r->imu_valid?2:0)|(r->imu.calibrated?4:0)|
         (r->imu.calibrating?8:0)|(r->imu.calibration_failed?16:0);
    put16(p+6,c->deviation);
    for(unsigned i=0;i<2;i++) {put16(p+8+i*2,c->speed[i]);put16(p+12+i*2,c->pwm[i]);}
    for(unsigned i=0;i<3;i++) {
        put16(p+16+i*2,r->imu.accel[i]);put16(p+22+i*2,r->imu.gyro_raw[i]);put16(p+28+i*2,r->imu.bias[i]);
    }
    for(unsigned i=0;i<4;i++) p[34+i]=(uint8_t)(now>>(8*i));
    put16(p+38,r->bad_frames);put16(p+40,r->dropped_tx);
    emit(r,0x81,p,sizeof p);
}
