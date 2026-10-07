#ifndef APP_CONFIG_F407_H
#define APP_CONFIG_F407_H
/* Classroom defaults; counts per 10 ms, never unverified rpm. */
#ifndef APP_DIAGNOSTIC
#define APP_DIAGNOSTIC 0
#endif
#define APP_HEARTBEAT_MS 500U
#define APP_SENSOR_MAX_AGE_MS 20U
#define APP_MAX_PWM 300
#define APP_STALL_PWM 150
#define APP_STALL_MS 500U
#define APP_ENCODER_LEFT_INVERT 0
#define APP_ENCODER_RIGHT_INVERT 0
#define APP_TRACK_ACTIVE_LOW 1
#define APP_TRACK_REVERSE 0
#define APP_TRACK_SIGN 1
#define APP_DEFAULT_PARAMS {5,20,5,20,2,0}
#endif
