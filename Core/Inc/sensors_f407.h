#ifndef SENSORS_F407_H
#define SENSORS_F407_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint16_t previous[2]; int8_t sign[2]; int32_t left, right; uint32_t last_ms, window_ms; bool valid; } Encoder;
void Encoder_Init(Encoder *e, uint16_t left, uint16_t right, bool invert_left, bool invert_right);
void Encoder_InitTimed(Encoder *,uint16_t,uint16_t,bool,bool,uint32_t);
bool Encoder_Poll(Encoder *,uint16_t,uint16_t,uint32_t);
void Encoder_Sample(Encoder *e, uint16_t left, uint16_t right);
typedef enum { SENSOR_IMU, SENSOR_TRACK } SensorDevice;
/* External SPI boundary: one complete CS transaction; false on timeout/error. */
typedef struct {
    bool (*transfer)(void *, SensorDevice, const uint8_t *, uint8_t *, uint16_t);
    void (*delay_ms)(void *, uint32_t);
    void *context;
} SensorBus;
typedef struct { uint8_t raw, mask; int16_t deviation; } TrackSample;
typedef struct {
    int16_t accel[3], gyro_raw[3], temperature;
    int32_t gyro[3], bias[3], sum[3];
    uint16_t samples;
    bool calibrating, calibrated, calibration_failed, fresh;
} Imu;
bool Track_Init(SensorBus *bus);
bool Track_Read(SensorBus *bus, TrackSample *sample);
bool Imu_Init(SensorBus *bus, Imu *imu);
bool Imu_Read(SensorBus *bus, Imu *imu);
void Imu_Calibrate(Imu *imu);
#endif
