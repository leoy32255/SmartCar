#include "bsp_config.h"
#include "host_assert.h"
static USART_TypeDef port;
UART_HandleTypeDef huart2={.Instance=&port};
#undef USART2
#define USART2 (&port)
#define __DMB() ((void)0)
#define __get_PRIMASK() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(x) ((void)(x))
#include "../Core/F407/uart.c"
int main(void) {
    uint8_t b;uint32_t t;
    for(unsigned i=0;i<256;i++) Uart_ReceiveISR((uint8_t)i,i,false);
    Uart_ReceiveISR(1,300,false);assert(Uart_TakeError());assert(!Uart_Read(&b,&t));
    Uart_ReceiveISR(9,500,false);assert(Uart_Read(&b,&t) && b==9 && t==500);
    Uart_ReceiveISR(5,501,true);assert(Uart_TakeError());assert(!Uart_Read(&b,&t));
    uint8_t packet[]={1,2,3};
    for(unsigned cycle=0;cycle<30000;cycle++) {
        assert(Uart_Send(0,packet,3));
        for(unsigned i=0;i<3;i++) {Uart_TransmitISR();assert(port.DR==packet[i]);}
        Uart_TransmitISR();assert(!(port.CR1&USART_CR1_TXEIE));
    }
    uint8_t full[256]={0};assert(Uart_Send(0,full,256));assert(!Uart_Send(0,packet,3));
    return 0;
}
