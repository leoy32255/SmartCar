#ifndef UART_F407_H
#define UART_F407_H
#include <stdint.h>
#include <stdbool.h>
void Uart_ReceiveISR(uint8_t,uint32_t,bool);
bool Uart_Read(uint8_t *,uint32_t *);
bool Uart_Send(void *,const uint8_t *,unsigned);
bool Uart_TakeError(void);
void Uart_TransmitISR(void);
#endif
