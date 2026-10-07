#include "bsp_config.h"
#include "sensor_port_f407.h"
#include "host_assert.h"
SPI_HandleTypeDef hspi2;
static int imu=1,track=1;
static HAL_StatusTypeDef result;
static SensorDevice expected;
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState state) {
    if(p==BSP_IMU_CS_PORT && pin==BSP_IMU_CS_PIN) imu=state;
    if(p==BSP_TRACK_CS_PORT && pin==BSP_TRACK_CS_PIN) track=state;
    assert(imu || track);
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *s,const uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout) {
    assert(s==&hspi2 && tx[0]==0x75 && n==3 && rx!=0);
    assert(expected==SENSOR_IMU ? (!imu && track) : (imu && !track));
    assert(imu != track && timeout<=2);
    return result;
}
void HAL_Delay(uint32_t ms) {(void)ms;}
int main(void) {
    uint8_t tx[3]={0x75},rx[3];
    for(int dev=0;dev<2;dev++) for(int err=0;err<4;err++) {
        expected=(SensorDevice)dev; result=(HAL_StatusTypeDef)err;
        assert(board_sensor_bus.transfer(0,(SensorDevice)dev,tx,rx,3)==(err==0));
        assert(imu && track);
    }
    return 0;
}
