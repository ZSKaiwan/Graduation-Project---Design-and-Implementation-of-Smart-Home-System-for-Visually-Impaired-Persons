#ifndef __GPS_USART_H
#define __GPS_USART_H

#include "stm32f10x.h"

// 串口配置宏定义
#define GPS_USARTx           USART2
#define GPS_USART_CLK        RCC_APB1Periph_USART2
#define GPS_GPIO_PORT        GPIOA
#define GPIO_CLK             RCC_APB2Periph_GPIOA
#define GPS_TX_PIN           GPIO_Pin_2   // PA2 复用推挽输出
#define GPS_RX_PIN           GPIO_Pin_3   // PA3 上拉输入
#define GPS_USART_IRQn       USART2_IRQn
#define GPS_USART_IRQHandler USART2_IRQHandler

// 接收缓冲区配置
#define GPS_REC_BUFFER_SIZE 256
extern u8 GPS_RecBuffer[GPS_REC_BUFFER_SIZE];  // GPS接收缓冲区
extern u8 GPS_Rec_Cnt;                         // 接收数据长度计数

// 函数声明
void GPS_USART_Init(void);                // GPS串口初始化
void GPS_USART_SendByte(u8 byte);         // 串口发送单个字节
void GPS_USART_SendString(u8 *str);       // 串口发送字符串

#endif

