#include "control_f407.h"
#include "app_config_f407.h"
static int32_t clip(int32_t x,int32_t lo,int32_t hi) { return x<lo?lo:x>hi?hi:x; }
void Control_Init(Control *c,uint32_t now) {
    *c=(Control){.last_tick=now,.param=APP_DEFAULT_PARAMS};
}
void Control_BeginBatch(Control *c) { c->stop_barrier=false; }
void Control_Stop(Control *c) {
    c->mode=MODE_STOP; c->stop_barrier=true; c->previous_deviation=0;
    for(unsigned i=0;i<2;i++) {
        c->target[i]=c->pwm[i]=0; c->integral[i]=c->previous_error[i]=0;
    }
}
void Control_Fault(Control *c,uint16_t f) { c->fault|=f; Control_Stop(c); }
void Control_CheckTime(Control *c,uint32_t now) {
    if(c->mode!=MODE_STOP && (!c->heartbeat_seen || now-c->heartbeat_ms>=APP_HEARTBEAT_MS))
        Control_Fault(c,FAULT_HEARTBEAT);
}
void Control_Heartbeat(Control *c,uint32_t now) {
    Control_CheckTime(c,now); c->heartbeat_ms=now; c->heartbeat_seen=true;
}
void Control_Sensors(Control *c,uint8_t mask,int16_t deviation,int16_t left,int16_t right,bool valid,uint32_t now) {
    c->mask=mask; c->deviation=deviation; c->speed[0]=left; c->speed[1]=right;
    c->sensors_valid=valid; c->sensor_ms=now;
    if(!valid) Control_Fault(c,FAULT_SENSOR);
}
bool Control_Clear(Control *c,uint32_t now) {
    if(c->mode!=MODE_STOP || !c->sensors_valid || now-c->sensor_ms>APP_SENSOR_MAX_AGE_MS ||
       !c->heartbeat_seen || now-c->heartbeat_ms>=APP_HEARTBEAT_MS) return false;
    c->fault=0; Control_Stop(c); return true;
}
bool Control_Start(Control *c,uint8_t mode,uint32_t now) {
    Control_CheckTime(c,now);
    if(mode<MODE_TRACK || mode>MODE_RC || c->stop_barrier || c->fault ||
       !c->sensors_valid || now-c->sensor_ms>APP_SENSOR_MAX_AGE_MS || !c->heartbeat_seen || now-c->heartbeat_ms>=APP_HEARTBEAT_MS)
        return false;
    Control_Stop(c); c->stop_barrier=false; c->mode=mode; c->last_tick=now;
    c->stall_since[0]=c->stall_since[1]=now;
    return true;
}
bool Control_Drive(Control *c,int16_t left,int16_t right) {
    if(c->mode!=MODE_RC || c->stop_barrier || left < -10 || left>10 || right < -10 || right>10) return false;
    c->target[0]=left; c->target[1]=right; return true;
}
bool Control_Param(Control *c,uint8_t id,int16_t value) {
    if(id>=6 || value<0 || value>(id==0 ? 20:200) || c->mode!=MODE_STOP) return false;
    c->param[id]=value; return true;
}
void Control_Tick(Control *c,uint32_t now) {
    Control_CheckTime(c,now);
    if(c->mode==MODE_STOP) { c->last_tick=now; return; }
    if(!c->sensors_valid || now-c->sensor_ms>APP_SENSOR_MAX_AGE_MS) { Control_Fault(c,FAULT_SENSOR); return; }
    if(now-c->last_tick>APP_CONTROL_MAX_GAP_MS) { Control_Fault(c,FAULT_SCHEDULE); return; }
    if(c->mode==MODE_TRACK) {
        if(!c->mask) { Control_Fault(c,FAULT_LINE); return; }
        /* Only a narrow contiguous 1/2-channel line is classified. Wider or
           split masks are ambiguous: no invented cross/T classification. */
        uint8_t m=c->mask;
        while(!(m&1U)) m>>=1;
        if(m!=1 && m!=3) { Control_Fault(c,FAULT_AMBIGUOUS); return; }
        if(now-c->last_tick<APP_SPEED_CONTROL_MS) return;
        int32_t turn=(c->param[1]*c->deviation+c->param[2]*(c->deviation-c->previous_deviation))/100;
        c->previous_deviation=c->deviation;
        int32_t base=c->param[0];
        if(c->deviation>=150 || c->deviation<=-150) base/=2;
        c->target[0]=(int16_t)clip(base+turn,0,20);
        c->target[1]=(int16_t)clip(base-turn,0,20);
    }
    if(now-c->last_tick<APP_SPEED_CONTROL_MS) return;
    c->last_tick=now;
    for(unsigned i=0;i<2;i++) {
        int32_t error=c->target[i]-c->speed[i];
        if(c->target[i]==0) { c->pwm[i]=0; c->integral[i]=0; c->previous_error[i]=0; }
        else {
            int32_t integral=clip(c->integral[i]+error*c->param[4],-APP_MAX_PWM,APP_MAX_PWM);
            int32_t output=error*c->param[3]+integral+(error-c->previous_error[i])*c->param[5];
            if(output>=-APP_MAX_PWM && output<=APP_MAX_PWM) c->integral[i]=integral;
            c->pwm[i]=(int16_t)clip(output,-APP_MAX_PWM,APP_MAX_PWM);
            c->previous_error[i]=error;
        }
        if((c->pwm[i]>=APP_STALL_PWM || c->pwm[i]<=-APP_STALL_PWM) && c->speed[i]==0) {
            if(now-c->stall_since[i]>=APP_STALL_MS) { Control_Fault(c,FAULT_STALL); return; }
        } else c->stall_since[i]=now;
    }
}
