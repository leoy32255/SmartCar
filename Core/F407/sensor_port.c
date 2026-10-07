#include "bsp_config.h"
#include "sensor_port_f407.h"
static bool bus_error;
bool SensorPort_TakeError(void) { bool error=bus_error;bus_error=false;return error; }
static bool transfer(void *context, SensorDevice device, const uint8_t *tx,
                     uint8_t *rx, uint16_t length) {
    (void)context;
    /* Main-loop only. No ISR accesses SPI2; always release both CS on failure. */
    HAL_GPIO_WritePin(BSP_IMU_CS_PORT,BSP_IMU_CS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(BSP_TRACK_CS_PORT,BSP_TRACK_CS_PIN,GPIO_PIN_SET);
    GPIO_TypeDef *port=device==SENSOR_IMU ? BSP_IMU_CS_PORT : BSP_TRACK_CS_PORT;
    uint16_t pin=device==SENSOR_IMU ? BSP_IMU_CS_PIN : BSP_TRACK_CS_PIN;
    HAL_GPIO_WritePin(port,pin,GPIO_PIN_RESET);
    HAL_StatusTypeDef result=HAL_SPI_TransmitReceive(&hspi2,tx,rx,length,2);
    HAL_GPIO_WritePin(port,pin,GPIO_PIN_SET);
    if(result!=HAL_OK) bus_error=true;
    return result==HAL_OK;
}
static void delay(void *context, uint32_t ms) { (void)context; HAL_Delay(ms); }
SensorBus board_sensor_bus={transfer,delay,0};
