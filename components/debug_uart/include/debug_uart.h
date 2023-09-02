/*! \file debug_uart.h
    \brief uart interface support header file */
#ifndef __DEBUG_UART_H__
#define __DEBUG_UART_H__

void debug_uart_init(void);
void debug_uart_tx_task(void *arg);
void debug_uart_rx_task(void *arg);

#endif