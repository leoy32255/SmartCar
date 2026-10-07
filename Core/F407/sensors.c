#include "sensors_f407.h"
#include "app_config_f407.h"
void Encoder_Init(Encoder *e, uint16_t left, uint16_t right, bool il, bool ir) {
    *e = (Encoder){.previous={left,right}, .sign={il ? -1 : 1, ir ? -1 : 1}};
}
static int32_t delta(uint16_t now, uint16_t old) {
    uint32_t d = (uint16_t)(now - old);
    return d < 32768U ? (int32_t)d : (int32_t)d - 65536;
}
void Encoder_Sample(Encoder *e, uint16_t left, uint16_t right) {
    e->left = delta(left, e->previous[0]) * e->sign[0];
    e->right = delta(right, e->previous[1]) * e->sign[1];
    e->previous[0] = left; e->previous[1] = right;
}

static bool reg_write(SensorBus *b, SensorDevice d, uint8_t reg, uint8_t value) {
    uint8_t tx[3] = {0}, rx[3] = {0};
    unsigned n = d == SENSOR_IMU ? 2 : 3;
    if (d == SENSOR_IMU) { tx[0]=reg; tx[1]=value; }
    else { tx[0]=0x40; tx[1]=reg; tx[2]=value; }
    return b->transfer(b->context,d,tx,rx,n);
}
static bool reg_read(SensorBus *b, SensorDevice d, uint8_t reg, uint8_t *out, unsigned count) {
    uint8_t tx[16] = {0}, rx[16] = {0};
    unsigned prefix = d == SENSOR_IMU ? 1 : 2;
    if (count + prefix > sizeof tx) return false;
    if (d == SENSOR_IMU) tx[0]=reg|0x80;
    else { tx[0]=0x41; tx[1]=reg; }
    if (!b->transfer(b->context,d,tx,rx,count+prefix)) return false;
    for(unsigned i=0;i<count;i++) out[i]=rx[i+prefix];
    return true;
}
bool Track_Init(SensorBus *b) {
    /* A0..2 tied low. Reset BANK even after an MCU-only restart. In BANK0,
       address 0x05 is GPINTENB, also safe to clear. No output is enabled. */
    if (!reg_write(b,SENSOR_TRACK,0x05,0) || !reg_write(b,SENSOR_TRACK,0x0a,0x08)) return false;
    const uint8_t reg[]={0x00,0x01,0x02,0x03,0x0c,0x0d};
    const uint8_t val[]={0xff,0xff,0,0,0xff,0xff};
    for(unsigned i=0;i<sizeof reg;i++) {
        uint8_t read;
        if (!reg_write(b,SENSOR_TRACK,reg[i],val[i]) ||
            !reg_read(b,SENSOR_TRACK,reg[i],&read,1) || read!=val[i]) return false;
    }
    return true;
}
bool Track_Read(SensorBus *b, TrackSample *s) {
    uint8_t cfg[2], raw;
    *s=(TrackSample){0};
    /* Pull-up readback detects a floating/disconnected expander, not J_LINE. */
    if (!reg_read(b,SENSOR_TRACK,0x0a,cfg,1) || cfg[0]!=8 ||
        !reg_read(b,SENSOR_TRACK,0x0c,cfg,2) || cfg[0]!=255 || cfg[1]!=255 ||
        !reg_read(b,SENSOR_TRACK,0x12,&raw,1)) return false;
    s->raw=raw; s->mask=(APP_TRACK_ACTIVE_LOW ? (uint8_t)(~raw):raw)&31;
    if(APP_TRACK_REVERSE) { uint8_t reversed=0; for(unsigned i=0;i<5;i++) if(s->mask&(1U<<i)) reversed|=1U<<(4-i); s->mask=reversed; }
    int sum=0, count=0;
    for(unsigned i=0;i<5;i++) if(s->mask & (1U<<i)) { sum+=(int)i*100-200; count++; }
    s->deviation=count ? APP_TRACK_SIGN*sum/count : 0;
    return true;
}
bool Imu_Init(SensorBus *b, Imu *s) {
    *s=(Imu){0};
    uint8_t id;
    if(!reg_read(b,SENSOR_IMU,0x75,&id,1) || id!=0x70 ||
       !reg_write(b,SENSOR_IMU,0x6b,0x80)) return false;
    b->delay_ms(b->context,100);
    const uint8_t reg[]={0x6b,0x6a,0x19,0x1a,0x1b,0x1c,0x1d,0x37,0x38};
    const uint8_t val[]={1,0x10,4,3,0,0,3,0x20,1};
    for(unsigned i=0;i<sizeof reg;i++) {
        if(!reg_write(b,SENSOR_IMU,reg[i],val[i])) return false;
    }
    b->delay_ms(b->context,50);
    for(unsigned i=0;i<sizeof reg;i++) {
        if(!reg_read(b,SENSOR_IMU,reg[i],&id,1) || id!=val[i]) return false;
    }
    return true;
}
void Imu_Calibrate(Imu *s) {
    s->calibrating=true; s->calibrated=false; s->calibration_failed=false; s->samples=0;
    for(unsigned i=0;i<3;i++) { s->sum[i]=0; s->bias[i]=0; }
}
static int16_t signed_be(const uint8_t *p) {
    uint32_t v=(uint32_t)p[0]*256+p[1];
    return (int16_t)(v<32768 ? (int32_t)v : (int32_t)v-65536);
}
bool Imu_Read(SensorBus *b, Imu *s) {
    uint8_t data[14], ready;
    s->fresh=false;
    if(!reg_read(b,SENSOR_IMU,0x3a,&ready,1)) return false;
    if(!(ready&1)) return true;
    if(!reg_read(b,SENSOR_IMU,0x3b,data,14)) return false;
    s->fresh=true;
    int64_t norm=0;
    bool stationary=true;
    for(unsigned i=0;i<3;i++) {
        s->accel[i]=signed_be(data+i*2);
        norm+=(int32_t)s->accel[i]*s->accel[i];
        s->gyro_raw[i]=signed_be(data+8+i*2);
        if(s->gyro_raw[i]>655 || s->gyro_raw[i]<-655) stationary=false;
    }
    s->temperature=signed_be(data+6);
    if(norm<201326592 || norm>335544320) stationary=false;
    if(s->calibrating) {
        if(!stationary) { s->calibrating=false; s->calibration_failed=true; }
        else {
            for(unsigned i=0;i<3;i++) s->sum[i]+=s->gyro_raw[i];
            if(++s->samples==200) {
                for(unsigned i=0;i<3;i++) s->bias[i]=s->sum[i]/200;
                s->calibrating=false; s->calibrated=true;
            }
        }
    }
    for(unsigned i=0;i<3;i++) s->gyro[i]=(int32_t)s->gyro_raw[i]-s->bias[i];
    return true;
}

void Encoder_InitTimed(Encoder *e,uint16_t l,uint16_t r,bool il,bool ir,uint32_t now) {
    Encoder_Init(e,l,r,il,ir);e->last_ms=now;e->valid=false;
}
bool Encoder_Poll(Encoder *e,uint16_t l,uint16_t r,uint32_t now) {
    uint32_t elapsed=now-e->last_ms;
    if(elapsed<10U) return false;
    Encoder_Sample(e,l,r);e->last_ms=now;e->window_ms=elapsed;e->valid=elapsed<=20U;
    if(e->valid) {e->left=e->left*10/(int32_t)elapsed;e->right=e->right*10/(int32_t)elapsed;}
    else e->left=e->right=0;
    return true;
}
