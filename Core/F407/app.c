#include "bsp_config.h"
#include "motor.h"
#include "sensor_port_f407.h"
#include "uart_f407.h"
#include "app_f407.h"
static Runtime runtime;
static Encoder encoder;
static bool imu_ok,track_ok;
static uint32_t ticks,sample_ms,imu_ms;
const Runtime *App_Status(void) {return &runtime;}
void App_Init(void) {
    Motor_Init();

    Runtime_Init(&runtime,HAL_GetTick(),Uart_Send,0);
    imu_ok=Imu_Init(&board_sensor_bus,&runtime.imu);
    track_ok=Track_Init(&board_sensor_bus);
    runtime.imu_valid=false;
    if(SensorPort_TakeError()) Control_Fault(&runtime.control,FAULT_SENSOR);

    Encoder_InitTimed(&encoder,(uint16_t)TIM3->CNT,(uint16_t)TIM4->CNT,
                 APP_ENCODER_LEFT_INVERT,APP_ENCODER_RIGHT_INVERT,HAL_GetTick());
    ticks=bsp_control_ticks;sample_ms=HAL_GetTick();imu_ms=sample_ms;
}
void App_Step(void) {

    uint8_t previous_mode=runtime.control.mode;
    uint16_t previous_fault=runtime.control.fault;
    uint32_t now=HAL_GetTick();
    Runtime_BeginBatch(&runtime,now);
    if(Uart_TakeError()) { Control_Fault(&runtime.control,FAULT_UART);runtime.used=0; }
    uint8_t byte;uint32_t stamp;
    /* Bounded drain: sustained RX cannot starve the control scheduler. */
    for(unsigned n=0;n<256 && Uart_Read(&byte,&stamp);n++) Runtime_Byte(&runtime,byte,stamp,HAL_GetTick());
    if(runtime.control.mode==MODE_STOP) Motor_Stop();
    uint32_t current_ticks=bsp_control_ticks;
    if(current_ticks!=ticks) {
        if(current_ticks-ticks>APP_CONTROL_MAX_GAP_MS/BSP_CTRL_PERIOD_MS && runtime.control.mode!=MODE_STOP)
            Control_Fault(&runtime.control,FAULT_SCHEDULE);
        ticks=current_ticks;now=HAL_GetTick();
        TrackSample track;
        bool valid=track_ok && Track_Read(&board_sensor_bus,&track);
        if(!valid) track=(TrackSample){0};
        (void)Encoder_Poll(&encoder,(uint16_t)TIM3->CNT,(uint16_t)TIM4->CNT,now);
        bool encoder_ready=encoder.window_ms!=0;
        valid=valid && (encoder.valid || !encoder_ready);
        if(runtime.calibration_requested) { Imu_Calibrate(&runtime.imu);runtime.calibration_requested=false; }
        if(imu_ok) {
            if(!Imu_Read(&board_sensor_bus,&runtime.imu)) {
                valid=false;runtime.imu_valid=false;
                if(runtime.imu.calibrating) {runtime.imu.calibrating=false;runtime.imu.calibration_failed=true;}
            }
            else if(runtime.imu.fresh) {imu_ms=now;runtime.imu_valid=true;}
            if(now-imu_ms>100U) {
                runtime.imu_valid=false;
                if(runtime.imu.calibrating) {runtime.imu.calibrating=false;runtime.imu.calibration_failed=true;}
            }
        }
        if(SensorPort_TakeError()) valid=false;
        if(encoder.left>32767 || encoder.left<-32767 || encoder.right>32767 || encoder.right<-32767) valid=false;
        if(encoder_ready || !valid)
            Control_Sensors(&runtime.control,track.mask,track.deviation,(int16_t)encoder.left,(int16_t)encoder.right,valid,now);
        else runtime.control.sensors_valid=false;
        sample_ms=now;
    }
    now=HAL_GetTick();
    if(now-sample_ms>APP_SENSOR_MAX_AGE_MS && runtime.control.mode!=MODE_STOP) Control_Fault(&runtime.control,FAULT_SCHEDULE);
    if(Uart_TakeError()) {Control_Fault(&runtime.control,FAULT_UART);runtime.used=0;}
    Control_Tick(&runtime.control,now);
    if((previous_mode!=MODE_STOP && runtime.control.mode==MODE_STOP) || previous_fault!=runtime.control.fault)
        Runtime_InvalidateMotion(&runtime,now);
#if APP_DIAGNOSTIC
    Control_Stop(&runtime.control);Motor_Stop();
#else
    if(runtime.control.mode==MODE_STOP) Motor_Stop();
    else {Motor_Enable(1);Motor_SetPWM(runtime.control.pwm[0],runtime.control.pwm[1]);}
#endif
    Runtime_Telemetry(&runtime,now);
}
