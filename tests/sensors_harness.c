#include "sensors_f407.h"
#include "host_assert.h"
static unsigned char mcp[256], imu[256];
static int fail, fail_reg=-1, corrupt_reg=-1;
static bool bank1;
static bool transfer(void *ctx, SensorDevice dev, const uint8_t *tx, uint8_t *rx, uint16_t n) {
    (void)ctx;
    if (fail) return false;
    unsigned char *r = dev == SENSOR_IMU ? imu : mcp;
    unsigned start = dev == SENSOR_IMU ? 1 : 2;
    unsigned reg = dev == SENSOR_IMU ? tx[0] & 127 : tx[1];
    if((int)reg==fail_reg) return false;
    if(dev==SENSOR_TRACK && bank1) {
        assert(reg==5 && n==3 && tx[0]==0x40 && tx[2]==0); bank1=false; return true;
    }
    bool read = dev == SENSOR_IMU ? (tx[0] & 128) != 0 : tx[0] == 0x41;
    for(unsigned i=start;i<n;i++) { if(read) { rx[i]=r[reg]; if((int)reg==corrupt_reg) rx[i]^=1; reg++; } else r[reg++]=tx[i]; }
    return true;
}
static void delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }
int main(void) {
    Encoder e;
    Encoder_Init(&e, 65530, 3, 0, 1);
    Encoder_Sample(&e, 4, 65529);
    assert(e.left == 10 && e.right == 10);
    Encoder_Sample(&e, 65530, 3);
    assert(e.left == -10 && e.right == -10);
    SensorBus bus = {transfer, delay, 0};
    TrackSample track;
    bank1=true; assert(Track_Init(&bus) && !bank1);
    assert(mcp[0] == 255 && mcp[1] == 255 && mcp[12] == 255 && mcp[13] == 255);
    mcp[18] = 0xfb;
    assert(Track_Read(&bus, &track) && track.mask == 4 && track.deviation == 0);
    mcp[18] = 0xfe;
    assert(Track_Read(&bus, &track) && track.mask == 1 && track.deviation == -200);
    mcp[18] = 0xff;
    assert(Track_Read(&bus, &track) && track.mask == 0);
    fail=1; assert(!Track_Read(&bus, &track));
    fail=0; mcp[12]=0; assert(!Track_Read(&bus, &track));
    fail_reg=1; assert(!Track_Init(&bus)); fail_reg=-1;
    corrupt_reg=0; assert(!Track_Init(&bus)); corrupt_reg=-1;
    assert(Track_Init(&bus));
    for(unsigned i=0;i<5;i++) {
        mcp[18]=(uint8_t)~(1U<<i);
        assert(Track_Read(&bus,&track) && track.mask==(1U<<i));
        mcp[18]&=31;
        assert(Track_Read(&bus,&track) && track.mask==(1U<<i));
    }
    mcp[18]=0xe7;assert(Track_Read(&bus,&track) && track.mask==24 && track.deviation==150);
    Encoder_InitTimed(&e,0,0,0,0,0xfffffff8U);
    assert(!Encoder_Poll(&e,9,9,1));
    assert(Encoder_Poll(&e,10,10,2) && e.left==10 && e.window_ms==10 && e.valid);
    assert(Encoder_Poll(&e,25,25,17) && e.left==10 && e.window_ms==15 && e.valid);
    assert(Encoder_Poll(&e,55,55,47) && !e.valid);
    Imu state;
    imu[117]=0x70;
    assert(Imu_Init(&bus, &state));
    imu[58]=1; imu[63]=0x40; /* +1g Z, +/-2g */
    imu[67]=0; imu[68]=131; /* +1 deg/s X */
    Imu_Calibrate(&state);
    for(unsigned i=0;i<199;i++) assert(Imu_Read(&bus, &state));
    assert(!state.calibrated && state.samples==199);
    imu[58]=0; assert(Imu_Read(&bus,&state) && !state.fresh && state.samples==199);
    imu[58]=1; fail_reg=0x3a; assert(!Imu_Read(&bus,&state) && !state.fresh && state.samples==199);
    fail_reg=0x3b; assert(!Imu_Read(&bus,&state) && !state.fresh && state.samples==199);
    fail_reg=-1; assert(Imu_Read(&bus,&state));
    assert(state.calibrated && state.bias[0] == 131 && state.gyro[0] == 0);
    Imu_Calibrate(&state); imu[67]=0x20;
    assert(Imu_Read(&bus, &state) && state.calibration_failed);
    imu[67]=0; imu[68]=0;
    Imu_Calibrate(&state); imu[63]=0; assert(Imu_Read(&bus,&state) && state.calibration_failed);
    imu[59]=0xff;imu[60]=0xff;imu[61]=0x80;imu[62]=0;
    imu[63]=0x7f;imu[64]=0xff;imu[65]=0xff;imu[66]=0xfe;
    imu[67]=0x80;imu[68]=0;imu[69]=0xff;imu[70]=0xff;imu[71]=0x7f;imu[72]=0xff;
    assert(Imu_Read(&bus,&state));
    assert(state.accel[0]==-1 && state.accel[1]==-32768 && state.accel[2]==32767 && state.temperature==-2);
    assert(state.gyro_raw[0]==-32768 && state.gyro_raw[1]==-1 && state.gyro_raw[2]==32767);
    corrupt_reg=0x19; assert(!Imu_Init(&bus,&state)); corrupt_reg=-1;
    fail_reg=0x6a; assert(!Imu_Init(&bus,&state)); fail_reg=-1;
    imu[117]=0xff; assert(!Imu_Init(&bus, &state));
    return 0;
}
