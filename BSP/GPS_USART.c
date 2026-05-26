#include "GPS_USART.h"

// 全局变量定义
u8 GPS_RecBuffer[GPS_REC_BUFFER_SIZE] = {0};  // GPS数据接收缓冲区
u8 GPS_Rec_Cnt = 0;                          // 接收数据长度

/**
 * @brief  GPS串口(USART2)初始化函数
 * @param  无
 * @retval 无
 * @note   波特率9600，8N1，开启接收中断
 */
void GPS_USART_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(GPIO_CLK, ENABLE);    // 使能GPIOA时钟
    RCC_APB1PeriphClockCmd(GPS_USART_CLK, ENABLE);// 使能USART2时钟

    // 2. 配置GPIO引脚
    // PA2 - USART2_TX 复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPS_TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPS_GPIO_PORT, &GPIO_InitStructure);

    // PA3 - USART2_RX 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPS_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;    // 上拉输入
    GPIO_Init(GPS_GPIO_PORT, &GPIO_InitStructure);

    // 3. 配置串口参数
   // USART_InitStructure.USART_BaudRate = 9600;                     // NEO-6M默认波特率9600
	USART_InitStructure.USART_BaudRate = 38400;                     // NEO-6M默认波特率9600
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;    // 8位数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;         // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;            // 无校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;// 收发模式
    USART_Init(GPS_USARTx, &USART_InitStructure);

    // 4. 配置NVIC中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = GPS_USART_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;  // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;         // 子优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. 使能串口接收中断
    USART_ITConfig(GPS_USARTx, USART_IT_RXNE, ENABLE);

    // 6. 使能串口
    USART_Cmd(GPS_USARTx, ENABLE);
}

/**
 * @brief  串口发送单个字节
 * @param  byte: 要发送的字节数据
 * @retval 无
 */
void GPS_USART_SendByte(u8 byte)
{
    while(USART_GetFlagStatus(GPS_USARTx, USART_FLAG_TXE) == RESET); // 等待发送缓冲区为空
    USART_SendData(GPS_USARTx, byte);                                // 发送数据
}

/**
 * @brief  串口发送字符串
 * @param  str: 字符串指针
 * @retval 无
 */
void GPS_USART_SendString(u8 *str)
{
    while(*str != '\0')
    {
        GPS_USART_SendByte(*str);
        str++;
    }
}

/**
 * @brief  USART2中断服务函数
 * @param  无
 * @retval 无
 * @note   接收GPS模块发送的NMEA协议数据
 */
void GPS_USART_IRQHandler(void)
{
    u8 res;
    // 接收中断
    if(USART_GetITStatus(GPS_USARTx, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(GPS_USARTx);  // 读取接收数据

        // 接收数据存入缓冲区，防止溢出
        if(GPS_Rec_Cnt < GPS_REC_BUFFER_SIZE - 1)
        {
            GPS_RecBuffer[GPS_Rec_Cnt++] = res;
        }
        else
        {
            GPS_Rec_Cnt = 0;  // 溢出清空缓冲区
        }
        USART_ClearITPendingBit(GPS_USARTx, USART_IT_RXNE); // 清除中断标志
    }
}
