#include "Usart.h"
#include "MQTT.h"
#include "Var.h"
#include "Delay.h"      // 提供 Delay_ms、Delay_GetMs
#include <string.h>
#include <stdio.h>

////u8 Send_slave[53]={0};//本地的气象数据
////服务器返回的数据
//u16 MQTT_Connect_receiv[4]={0}; //收到的数据，Connect已连接四个字节
//u16 MQTT_Set_true[4]={0}; //收到的数据，收到的控制数据为真
//u16 MQTT_Set_fals[4]={0}; //收到的数据，收到的控制数据为假
//u8  MQTT_Connect_cnt=0,MQTT_Set_true_cnt=0,MQTT_Set_fals_cnt=0;//数组计数
//// /**
////  * @brief  USART GPIO 配置,工作参数配置
////  * @param  无
////  * @retval 无
////  */
////	static void NVIC_Configuration(void)
////{
////  NVIC_InitTypeDef NVIC_InitStructure;
////  
////  /* 嵌套向量中断控制器组选择 */
////  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
////	
////		/* 配置USART为中断源 */
////  NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART3_IRQ;
////  /* 抢断优先级*/
////  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
////  /* 子优先级 */
////  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
////  /* 使能中断 */
////  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
////  /* 初始化配置NVIC */
////  NVIC_Init(&NVIC_InitStructure);
////	

////}

////void USART_Config(void)
////{
////	GPIO_InitTypeDef GPIO_InitStructure;
////    USART_InitTypeDef USART_InitStructure;

////    // 开时钟
////    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

////    // PA9 TX 复用推挽
////    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
////    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
////    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
////    GPIO_Init(GPIOA, &GPIO_InitStructure);

////    // PA10 RX 浮空输入
////    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
////    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
////    GPIO_Init(GPIOA, &GPIO_InitStructure);

////    // 串口配置
////    USART_InitStructure.USART_BaudRate = 115200;
////    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
////    USART_InitStructure.USART_StopBits = USART_StopBits_1;
////    USART_InitStructure.USART_Parity = USART_Parity_No;
////    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
////    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

////    USART_Init(USART1, &USART_InitStructure);
////    USART_Cmd(USART1, ENABLE);	    
////}

////void USART3_Config(void)
////{
////	GPIO_InitTypeDef GPIO_InitStructure;
////	USART_InitTypeDef USART_InitStructure;

////	// 打开串口GPIO的时钟
////	DEBUG_USART3_GPIO_APBxClkCmd(DEBUG_USART3_GPIO_CLK, ENABLE);
////	
////	// 打开串口外设的时钟
////	DEBUG_USART3_APBxClkCmd(DEBUG_USART3_CLK, ENABLE);

////	// 将USART Tx的GPIO配置为推挽复用模式
////	GPIO_InitStructure.GPIO_Pin = DEBUG_USART3_TX_GPIO_PIN;
////	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
////	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
////	GPIO_Init(DEBUG_USART3_TX_GPIO_PORT, &GPIO_InitStructure);

////  // 将USART Rx的GPIO配置为浮空输入模式
////	GPIO_InitStructure.GPIO_Pin = DEBUG_USART3_RX_GPIO_PIN;
////	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
////	GPIO_Init(DEBUG_USART3_RX_GPIO_PORT, &GPIO_InitStructure);
////	
////	// 配置串口的工作参数
////	// 配置波特率
////	USART_InitStructure.USART_BaudRate = DEBUG_USART3_BAUDRATE;
////	// 配置 针数据字长
////	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
////	// 配置停止位
////	USART_InitStructure.USART_StopBits = USART_StopBits_1;
////	// 配置校验位
////	USART_InitStructure.USART_Parity = USART_Parity_No ;
////	// 配置硬件流控制
////	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
////	// 配置工作模式，收发一起
////	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
////	// 完成串口的初始化配置
////	USART_Init(DEBUG_USART3x, &USART_InitStructure);

////	
////	// 串口中断优先级配置
////	NVIC_Configuration();
////	
////	// 使能串口接收中断
////	USART_ITConfig(DEBUG_USART3x, USART_IT_RXNE, ENABLE);	
////	
////	// 使能串口
////	USART_Cmd(DEBUG_USART3x, ENABLE);		  
////}

/////重定向c库函数printf到串口，重定向后可使用printf函数
//int fputc(int ch, FILE *f)
//{
//		/* 发送一个字节数据到串口 */
//		USART_SendData(DEBUG_USARTx, (uint8_t) ch);
//		
//		/* 等待发送完毕 */
//		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TXE) == RESET);		
//	
//		return (ch);
//}


/////重定向c库函数scanf到串口，重写向后可使用scanf、getchar等函数
//int fgetc(FILE *f)
//{
//		/* 等待串口输入数据 */
//		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);

//		return (int)USART_ReceiveData(DEBUG_USARTx);
//}

///*****************  发送一个字节 **********************/
//void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch)
//{
//	/* 发送一个字节数据到USART */
//	USART_SendData(pUSARTx,ch);
//		
//	/* 等待发送数据寄存器为空 */
//	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
//}

///*****************  发送字符串 **********************/
//void Usart_SendString( USART_TypeDef * pUSARTx, char *str)
//{
//	unsigned int k=0;
//  do 
//  {
//      Usart_SendByte( pUSARTx, *(str + k) );
//      k++;
//  } while(*(str + k)!='\0');
//  
//  /* 等待发送完成 */
//  while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
//  {}
//}

/////*这个代码是把组成的数据发送到服务器中*/
////void USART3_SendData(uint8_t *data, uint16_t len) 
////{
////	unsigned int i=0;
////  for (i = 0; i < len; i++) 
////		{ // 遍历数据缓冲区
////    USART_SendData(USART3, data[i]);  // 发送一个字节数据
////    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);  // 等待发送完成
////    }
////}

////void DEBUG_USART3_IRQHandler(void)
////{
//    uint8_t ucTemp;

//    // 注意：这里必须是 USART3，因为你的 ESP8266 接在串口3
//    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
//    {
//        // 【只读1次！！！】
//        ucTemp = USART_ReceiveData(USART3);
//	
//        // ==============================================
//        // 1. 判断 MQTT 登录成功（标准应答：0x20 02 00 00）
//        // ==============================================
//        static uint8_t mqtt_state = 0;
//        if(mqtt_state == 0 && ucTemp == 0x20)
//        {
//            mqtt_state = 1;
//        }
//        else if(mqtt_state == 1 && ucTemp == 0x02)
//        {
//            mqtt_state = 2;
//        }
//        else if(mqtt_state == 2 && ucTemp == 0x00)
//        {
//            mqtt_state = 3;
//        }
//        else if(mqtt_state == 3 && ucTemp == 0x00)
//        {
//            mqtt_state = 0;
//            Start_flag = 1;      // ✅ MQTT 登录成功
//        }
//        else
//        {
//            mqtt_state = 0;      // 匹配错误，复位
//        }

//        // ==============================================
//        // 2. 匹配指令：true （开启）
//        // ==============================================
//        static uint8_t cmd_true = 0;
//        if(cmd_true == 0 && ucTemp == 't')
//            cmd_true = 1;
//        else if(cmd_true == 1 && ucTemp == 'r')
//            cmd_true = 2;
//        else if(cmd_true == 2 && ucTemp == 'u')
//            cmd_true = 3;
//        else if(cmd_true == 3 && ucTemp == 'e')
//        {
//            cmd_true = 0;
//            Contrue_flag = 1;   // ✅ 收到开启指令
//        }
//        else if(cmd_true != 0)
//        {
//            cmd_true = 0;       // 出错复位
//        }

//        // ==============================================
//        // 3. 匹配指令：false （关闭）
//        // ==============================================
//        static uint8_t cmd_false = 0;
//        if(cmd_false == 0 && ucTemp == 'f')
//            cmd_false = 1;
//        else if(cmd_false == 1 && ucTemp == 'a')
//            cmd_false = 2;
//        else if(cmd_false == 2 && ucTemp == 'l')
//            cmd_false = 3;
//        else if(cmd_false == 3 && ucTemp == 's')
//            cmd_false = 4;
//        else if(cmd_false == 4 && ucTemp == 'e')
//        {
//            cmd_false = 0;
//            Conflas_flag = 1;   // ✅ 收到关闭指令
//        }
//        else if(cmd_false != 0)
//        {
//            cmd_false = 0;      // 出错复位
//        }
//    }
//}

