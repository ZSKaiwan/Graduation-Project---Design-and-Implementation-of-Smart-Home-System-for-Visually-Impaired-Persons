#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

/*-------------------------- 硬件引脚定义 --------------------------*/
#define ESP_USART       USART1
#define ESP_USART_CLK   RCC_APB2Periph_USART1
#define ESP_GPIO_CLK    RCC_APB2Periph_GPIOA
#define ESP_TX_PORT     GPIOA
#define ESP_TX_PIN      GPIO_Pin_9
#define ESP_RX_PORT     GPIOA
#define ESP_RX_PIN      GPIO_Pin_10

/*-------------------------- 函数声明 --------------------------*/
void ESP8266_Init(void);
int8_t ESP8266_ConnectWiFi(char *ssid, char *password);
int8_t ESP8266_ConnectMQTTServer(void);
void ESP8266_SendRawData(uint8_t *data, uint16_t len);
void ESP8266_ExitTransparent(void);

// ====== 非透传模式发送函数 ======
int8_t ESP8266_SendData(uint8_t *data, uint16_t len);   // 非透传发送，自动处理AT+CIPSEND
// 检查 TCP 连接是否存活（返回 1 表示连接正常，0 表示已断开）
uint8_t ESP8266_IsConnected(void);
void ESP8266_ProcessIPD(void);


/* ====== 供外部调用的发送字符串函数 ====== */
void ESP_SendString(char *str);
/* =========================================== */


#endif



