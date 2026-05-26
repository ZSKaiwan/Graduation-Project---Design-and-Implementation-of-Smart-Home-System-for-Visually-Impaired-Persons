#include "stm32f10x.h"
#include "PWM.h"
#include "Action.h"
#include "Var.h"
void Voice_Init(void)
{
	// ========== 添加部分：开启 AFIO 时钟（必须） ==========
    // 原代码缺少 AFIO 时钟，导致引脚重映射配置无效
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    // 语音串口配置
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  // 开启GPIOB时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); // 开启USART3时钟（在APB1总线上）

    // TX引脚配置：PB10
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;       // 复用推挽输出
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;           // USART3_TX (PB10)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // RX引脚配置：PB11
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;         // 上拉输入
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;           // USART3_RX (PB11)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	 // ========== 添加部分：强制 USART3 使用默认引脚 PB10/PB11 ==========
    // 关闭所有可能的重映射选项，确保不会意外映射到 PC10/PC11 或 PD8/PD9
    GPIO_PinRemapConfig(GPIO_FullRemap_USART3,    DISABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, DISABLE);
	
    // USART3 初始化
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate            = 9600;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART3, &USART_InitStructure);

    // 使能接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    // 中断优先级分组（通常只需在main中设置一次，此处保留以保持独立）
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 配置USART3中断
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_Init(&NVIC_InitStructure);

    // 使能USART3
    USART_Cmd(USART3, ENABLE);
}

// 语音串口中断处理函数
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        uint8_t rec_data = USART_ReceiveData(USART3);  // 读取接收到的数据

        if (rec_data == 0x29)       // 开窗命令
        {
            Window_Mode = 1;
            Servo = 1;
        }
        else if (rec_data == 0x30)  // 关窗命令
        {
            Window_Mode = 0;
            Servo = 0;
        }

        USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 清除中断标志位
    }
}

