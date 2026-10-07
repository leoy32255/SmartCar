#include "bsp_config.h"
#include "app_f407.h"
#include "host_assert.h"
#include <string.h>
#define __DMB() ((void)0)
#define __get_PRIMASK() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(x) ((void)(x))
#include "../Core/F407/uart.c"
static uint32_t now;
static uint8_t mcp[256],imu[256];
static bool spi_error;
uint32_t HAL_GetTick(void) {return now;}
void HAL_IncTick(void) {++now;}
void HAL_Delay(uint32_t ms) {now+=ms;}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *s,const uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout) {
    (void)s;(void)timeout;
    if(spi_error) return HAL_TIMEOUT;
    bool is_imu=(GPIOB->ODR&GPIO_PIN_12)==0;
    uint8_t *registers=is_imu?imu:mcp;
    unsigned prefix=is_imu?1:2,reg=is_imu?(tx[0]&127):tx[1];
    bool read=is_imu?(tx[0]&128)!=0:tx[0]==0x41;
    for(unsigned i=prefix;i<n;i++) {if(read)rx[i]=registers[reg++];else registers[reg++]=tx[i];}
    return HAL_OK;
}
extern void USART2_IRQHandler(void);
static void input(const uint8_t *data,unsigned n) {
    for(unsigned i=0;i<n;i++) {USART2->SR=USART_SR_RXNE;USART2->DR=data[i];USART2_IRQHandler();}
    USART2->SR=0;
}
static void command(uint8_t cmd,const uint8_t *data,unsigned n) {
    uint8_t bytes[60];unsigned count=Protocol_Frame(cmd,data,n,bytes);input(bytes,count);App_Step();
}
static void step(unsigned ms) {now+=ms;bsp_control_ticks+=ms/5;App_Step();}
static void stopped(void) {assert(TIM1->CCR1==0 && TIM1->CCR2==0);}
void test_app(void) {
    imu[117]=0x70;mcp[18]=0xfb;imu[58]=0;TIM3->CNT=TIM4->CNT=0;
    App_Init();App_Step();stopped();assert(!App_Status()->imu_valid);
    uint8_t mode=1;command(6,0,0);command(1,&mode,1);stopped();
    step(5);assert(!App_Status()->control.sensors_valid);
    step(5);imu[58]=1;imu[63]=0x40;step(5);assert(App_Status()->imu_valid);
    command(6,0,0);mode=2;command(1,&mode,1);
    uint8_t drive[]={5,0,5,0};command(2,drive,4);step(10);
#if APP_DIAGNOSTIC
    stopped();
#else
    assert(TIM1->CCR1>0);
#endif
#if !APP_DIAGNOSTIC
    /* ISR -> parser -> app -> real Motor: moving at 499, stopped at 500/501. */
    for(unsigned moving=1;moving<=2;moving++) {
        mode=0;command(1,&mode,1);step(5);command(6,0,0);uint32_t hb=now;
        mode=(uint8_t)moving;command(1,&mode,1);if(moving==2)command(2,drive,4);
        while(now-hb<495U) step(5);
        step(4);assert(now-hb==499U && App_Status()->control.mode==moving && TIM1->CCR1>0);
        step(1);stopped();assert(App_Status()->control.fault&FAULT_HEARTBEAT);
        step(1);stopped();step(9);command(6,0,0);command(7,0,0);stopped();step(5);
    }
    command(6,0,0);mode=2;command(1,&mode,1);command(2,drive,4);step(10);
    assert(TIM1->CCR1>0);step(30);stopped();assert(App_Status()->control.fault&FAULT_SENSOR);
    step(10);command(6,0,0);command(7,0,0);step(5);mode=2;command(1,&mode,1);command(2,drive,4);step(10);
    assert(TIM1->CCR1>0);uint8_t pad=0;while(Uart_Send(0,&pad,1)) {}
    uint32_t dropped=App_Status()->dropped_tx;mode=0;command(1,&mode,1);
    stopped();assert(App_Status()->dropped_tx>dropped);
    /* Release TX by the actual IRQ; no direct edits to queue state. */
    for(unsigned i=0;i<257;i++){USART2->SR=USART_SR_TXE;USART2_IRQHandler();}
    USART2->SR=0;step(5);command(6,0,0);mode=2;command(1,&mode,1);command(2,drive,4);step(10);
#endif
    mode=0;uint8_t stop_frame[8],start_frame[8];unsigned ns=Protocol_Frame(1,&mode,1,stop_frame);
    mode=1;unsigned nt=Protocol_Frame(1,&mode,1,start_frame);
    input(stop_frame,ns);input(start_frame,3);App_Step();stopped();
    now++;input(start_frame+3,nt-3);App_Step();stopped();assert(App_Status()->control.mode==MODE_STOP);
    step(5);command(6,0,0);mode=2;command(1,&mode,1);command(2,drive,4);step(10);
    spi_error=true;step(5);stopped();assert(App_Status()->control.fault&FAULT_SENSOR);
    spi_error=false;step(10);command(6,0,0);command(7,0,0);stopped();
    step(5);command(4,0,0);step(5);assert(App_Status()->imu.calibrating);
    imu[58]=0;for(unsigned i=0;i<21;i++)step(5);
    assert(!App_Status()->imu_valid && App_Status()->imu.calibration_failed);stopped();
    uint8_t part[]={0xaa,0x55,1};input(part,3);App_Step();
    USART2->SR=USART_SR_ORE;USART2_IRQHandler();USART2->SR=0;App_Step();
    assert(App_Status()->control.fault&FAULT_UART && App_Status()->used==0);stopped();
    step(30);assert(!App_Status()->control.sensors_valid);stopped();
    uint8_t fill=0;while(Uart_Send(0,&fill,1)) {}
    uint32_t drops=App_Status()->dropped_tx;step(100);assert(App_Status()->dropped_tx>drops);stopped();
}
