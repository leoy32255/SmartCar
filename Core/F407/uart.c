#include "bsp_config.h"
#include "uart_f407.h"
#define CAPACITY 256U
static uint8_t rx[CAPACITY],tx[CAPACITY];
static uint32_t timestamp[CAPACITY];
static volatile uint16_t rh,rt,th,tt;
static volatile bool failed;
void Uart_ReceiveISR(uint8_t value,uint32_t now,bool error) {
    if(error || (uint16_t)(rh-rt)==CAPACITY) { failed=true;return; }
    rx[rh&255]=value;timestamp[rh&255]=now;__DMB();++rh;
}
bool Uart_Read(uint8_t *value,uint32_t *stamp) {
    if(rt==rh) return false;
    __DMB();*value=rx[rt&255];*stamp=timestamp[rt&255];__DMB();++rt;return true;
}
bool Uart_TakeError(void) {
    uint32_t primask=__get_PRIMASK();__disable_irq();
    bool value=failed;failed=false;
    if(value) rt=rh;
    __set_PRIMASK(primask);return value;
}
bool Uart_Send(void *ctx,const uint8_t *data,unsigned n) {
    (void)ctx;
    if(n>CAPACITY-(uint16_t)(th-tt)) return false;
    uint16_t head=th;
    for(unsigned i=0;i<n;i++) tx[(head+i)&255]=data[i];
    __DMB();th=head+n;__HAL_UART_ENABLE_IT(&huart2,UART_IT_TXE);return true;
}
void Uart_TransmitISR(void) {
    if(tt!=th) {__DMB();USART2->DR=tx[tt&255];++tt;}
    else __HAL_UART_DISABLE_IT(&huart2,UART_IT_TXE);
}
