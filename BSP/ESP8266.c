/**
 * @file ESP8266.c
 * @brief ESP8266 WiFi模块驱动（使用硬件USART1，PA9-TX, PA10-RX）
 * @note  支持AT指令、透传模式、WiFi连接、TCP连接至OneNET服务器
 */

#include "ESP8266.h"
#include "Delay.h"      // 提供 Delay_ms、Delay_GetMs
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Buzzer.h"
#include "Var.h"
#include "MQTT.h"
/*-------------------------- 接收缓冲区（用于解析AT响应）--------------------------*/
static char esp_rx_buffer[512];
static uint16_t esp_rx_index = 0;
// 在 ESP8266.c 顶部增加一个供主循环读取的缓冲区及相关标志
volatile static char g_ipd_buffer[1024];      // 存储完整的 +IPD 数据块
volatile static uint16_t g_ipd_len = 0;       // 当前 +IPD 数据长度
volatile static uint8_t g_ipd_ready = 0;      // 数据就绪标志，主循环读取后清零


//清空缓冲区函数
static void ESP_ClearRxBuffer(void)
{
    memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
    esp_rx_index = 0;
}

/**
 * @brief 初始化USART1硬件（波特率115200，8N1，使能接收中断）
 * @param baudrate 波特率（通常115200）
 */
static void USART1_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    
    // 使能GPIOA和USART1时钟
    RCC_APB2PeriphClockCmd(ESP_GPIO_CLK | ESP_USART_CLK, ENABLE);
    
    // 配置TX引脚（PA9）为复用推挽输出
    GPIO_InitStruct.GPIO_Pin = ESP_TX_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ESP_TX_PORT, &GPIO_InitStruct);
    
    // 配置RX引脚（PA10）为浮空输入
    GPIO_InitStruct.GPIO_Pin = ESP_RX_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ESP_RX_PORT, &GPIO_InitStruct);
    
    // USART1参数配置：115200波特率，8位数据，1停止位，无校验，无硬件流控
    USART_InitStruct.USART_BaudRate = baudrate;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ESP_USART, &USART_InitStruct);
    
    // 使能接收中断（用于接收ESP8266返回的数据）
    USART_ITConfig(ESP_USART, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);        // 使能NVIC中断
    USART_Cmd(ESP_USART, ENABLE);       // 使能USART1
}

/**
 * @brief 发送单个字节（阻塞直到发送完成）
 * @param byte 待发送的字节
 */
static void ESP_SendByte(uint8_t byte)
{
    USART_SendData(ESP_USART, byte);
    while (USART_GetFlagStatus(ESP_USART, USART_FLAG_TXE) == RESET);
}

/**
 * @brief 发送字符串（自动添加\r\n需手动拼接）
 * @param str 以'\0'结尾的字符串
 */
void ESP_SendString(char *str)
{
    while (*str) {
        ESP_SendByte(*str++);
    }
}

void USART1_IRQHandler(void)
{
      if (USART_GetITStatus(ESP_USART, USART_IT_RXNE) != RESET) {
        uint8_t ch = USART_ReceiveData(ESP_USART);

        // 存入 AT 指令响应缓冲区（供 ESP_WaitForStr 等使用）
        if (esp_rx_index < sizeof(esp_rx_buffer) - 1) {
            esp_rx_buffer[esp_rx_index++] = ch;
        }

        // ====== 状态机：精确解析 +IPD 头部，提取数据长度和原始数据 ======
        static uint8_t  ipd_state = 0;
        static uint16_t ipd_len = 0;
        static uint16_t ipd_cnt = 0;
        static char     len_buf[8];
        static uint8_t  len_idx = 0;

        switch (ipd_state) {
            case 0: // 等待 "+IPD,"
                {
                    static const char *header = "+IPD,";
                    static uint8_t match_idx = 0;
                    if (ch == header[match_idx]) {
                        match_idx++;
                        if (match_idx == 5) {
                            ipd_state = 1;
                            match_idx = 0;
                            len_idx = 0;
                            ipd_len = 0;
                        }
                    } else {
                        match_idx = 0;
                    }
                }
                break;

            case 1: // 读取长度直到 ':'
                if (ch >= '0' && ch <= '9') {
                    if (len_idx < sizeof(len_buf) - 1)
                        len_buf[len_idx++] = ch;
                } else if (ch == ':') {
                    len_buf[len_idx] = '\0';
                    ipd_len = atoi(len_buf);
                    ipd_cnt = 0;
                    ipd_state = 2;
                } else {
                    ipd_state = 0;  // 格式错误，重置
                }
                break;

           case 2: // 读取 ipd_len 字节的数据
				 if (ipd_cnt < sizeof(g_ipd_buffer) - 1) {
					g_ipd_buffer[ipd_cnt++] = ch;
				}
				if (ipd_cnt >= ipd_len) {
					g_ipd_buffer[ipd_cnt] = '\0';
					g_ipd_len = ipd_len;
					g_ipd_ready = 1;   // 通知主循环处理
					ipd_state = 0;
				}
				break;
        }

        // 如果之前注册了回调（如 MQTT_ReceiveByte），这里仍可调用，但我们现在不需要
        // ParseIpdByte(ch);   // 若不再使用可注释掉
    }
}
/**
 * @brief 等待ESP8266返回指定字符串（超时返回0）
 * @param expected   期望的字符串，如 "GOT IP", "CONNECT OK", ">"
 * @param timeout_ms 超时时间（毫秒）
 * @retval 1 成功收到，0 超时
 */
static uint8_t ESP_WaitForStr(char *expected, uint32_t timeout_ms)
{
    uint32_t start = Delay_GetMs();
    while (Delay_GetMs() - start < timeout_ms) {
        if (strstr(esp_rx_buffer, expected) != NULL) {
            ESP_ClearRxBuffer();   // 清空缓冲区，准备下次使用
            return 1;
        }
        Delay_ms(10);
    }
    ESP_ClearRxBuffer();
    return 0;
}
/**
 * @brief ESP8266初始化：复位、测试AT、设置WiFi模式、配置AP热点
 * @note 执行后ESP8266处于AT指令模式，尚未连接WiFi和服务器
 */
void ESP8266_Init(void)
{
	
    USART1_Init(115200);                        // 初始化串口
	ESP8266_ExitTransparent();
    ESP_SendString("AT+RST\r\n");               // 复位模块
    Delay_ms(1000);
    ESP_SendString("AT\r\n");                   // AT测试
    Delay_ms(500);
	ESP_SendString("AT+CWMODE=1\r\n");          // 设置WiFi模式：Station
	Delay_ms(500);

    Delay_ms(2000);
}

/**
 * @brief 连接WiFi热点
 * @param ssid     热点名称
 * @param password 密码
 * @return 0:成功（简化版，未检查响应）
 */
int8_t ESP8266_ConnectWiFi(char *ssid, char *password)
{
	char cmd[128];
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        ESP_ClearRxBuffer();
        ESP_SendString(cmd);
        ESP_SendString("\r\n");
        
        // 等待 "GOT IP" 或 "WIFI CONNECTED"（最长20秒）
        if (ESP_WaitForStr("GOT IP", 20000) || ESP_WaitForStr("WIFI CONNECTED", 20000)) {
            Delay_ms(1000);   // 连接成功后额外稳定延时
            return 0;         // 成功
        }
        Delay_ms(1000);       // 重试前等待1秒
    }
    return -1;                // 失败
}

/**
 * @brief 建立TCP连接至OneNET服务器（非透传模式，仅建立连接）
 * @return 0:成功，-1:失败
 */
int8_t ESP8266_ConnectMQTTServer(void)
{
	Delay_ms(500);
    
    // 先关闭可能残留的旧连接
    ESP_ClearRxBuffer();
    ESP_SendString("AT+CIPCLOSE\r\n");
    Delay_ms(500);
    
    uint8_t tcp_ok = 0;
    for (uint8_t i = 0; i < 3; i++) {
        ESP_ClearRxBuffer();
        ESP_SendString("AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n");
        
        uint32_t start = Delay_GetMs();
        uint8_t got_connect = 0, got_ok = 0;
        while (Delay_GetMs() - start < 15000) {
            if (strstr(esp_rx_buffer, "CONNECT") != NULL) got_connect = 1;
            if (strstr(esp_rx_buffer, "OK") != NULL) got_ok = 1;
            // 如果遇到 ALREADY CONNECTED，也视为成功
            if (strstr(esp_rx_buffer, "ALREADY CONNECTED") != NULL) {
                tcp_ok = 1;
                break;
            }
            if (got_connect && got_ok) {
                tcp_ok = 1;
                break;
            }
            Delay_ms(10);
        }
        if (tcp_ok) break;
        Delay_ms(1000);
    }
    if (!tcp_ok) return -1;
    
    Delay_ms(1000);
    
    // 确认连接状态为 "STATUS:3"（已建立 TCP 连接）
    ESP_ClearRxBuffer();
    ESP_SendString("AT+CIPSTATUS\r\n");
    if (!ESP_WaitForStr("STATUS:3", 3000)) {
        // 状态异常，返回失败
        return -1;
    }
    
    Delay_ms(1000);
    return 0;
}
/**
 * @brief 透传模式下发送原始数据（直接通过USART1发送）
 * @param data 数据指针
 * @param len  数据长度
 */
void ESP8266_SendRawData(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        ESP_SendByte(data[i]);
    }
}

/**
 * @brief 退出透传模式（发送"+++"，无需换行）
 */
void ESP8266_ExitTransparent(void)
{
    ESP_SendString("+++");
    Delay_ms(500);          // ====== 适当延长等待时间 ======
}
/**
 * @brief 非透传模式下发送数据（通过AT+CIPSEND指令）
 * @param data  待发送的数据指针
 * @param len   数据长度
 * @return 0:成功，-1:失败
 * @note  此函数适用于单连接模式（AT+CIPMUX=0），不需要指定link_id
 */
int8_t ESP8266_SendData(uint8_t *data, uint16_t len)
{
    char cmd[32];
    
    // 1. 检查连接状态
    ESP_ClearRxBuffer();
    ESP_SendString("AT+CIPSTATUS\r\n");
    if (!ESP_WaitForStr("STATUS:3", 1000)) {
        // 连接失效，重连
        if (ESP8266_ConnectMQTTServer() != 0) return -1;
        Delay_ms(1000);
    }
    
    // 2. 发送 CIPSEND
    sprintf(cmd, "AT+CIPSEND=%d\r\n", len);
    ESP_ClearRxBuffer();
    ESP_SendString(cmd);
    
    uint32_t start = Delay_GetMs();
    uint8_t got_prompt = 0;
    while (Delay_GetMs() - start < 2000) {
        if (strstr(esp_rx_buffer, ">") != NULL) {
            got_prompt = 1;
            break;
        }
        // 检测到连接关闭或无效
        if (strstr(esp_rx_buffer, "CLOSED") != NULL ||
            strstr(esp_rx_buffer, "link is not valid") != NULL) {
            // 重连
            ESP8266_ConnectMQTTServer();
            Delay_ms(1000);
            // 重新发送命令
            ESP_ClearRxBuffer();
            ESP_SendString(cmd);
            start = Delay_GetMs();
        }
        Delay_ms(10);
    }
    if (!got_prompt) return -1;
    
    // 3. 发送数据
    for (uint16_t i = 0; i < len; i++) {
        ESP_SendByte(data[i]);
    }
    
    // 4. 等待 SEND OK
    if (!ESP_WaitForStr("SEND OK", 2000)) {
        return -1;
    }
    return 0;
}
/**
 * @brief 检查 TCP 连接是否存活
 * @return 1: 连接正常（STATUS:3），0: 已断开或状态未知
 */
uint8_t ESP8266_IsConnected(void)
{
    ESP_ClearRxBuffer();
    ESP_SendString("AT+CIPSTATUS\r\n");
    return ESP_WaitForStr("STATUS:3", 1000);
}
/**
 * @brief 主循环中调用：检查是否有收到的 +IPD 数据并处理
 * @note 该函数会执行舵机控制和响应发送，应在主循环中周期性调用
 */
void ESP8266_ProcessIPD(void)
{
    if (!g_ipd_ready) return;

    // 将原始 MQTT 报文逐字节送入状态机，它会解析 PUBLISH 并触发 on_property_set
    for (uint16_t i = 0; i < g_ipd_len; i++) {
        MQTT_ReceiveByte((uint8_t)g_ipd_buffer[i]);
    }		
	
    g_ipd_ready = 0;   // 处理完毕，清标志
}

