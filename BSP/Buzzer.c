#include "Buzzer.h"
#include "Var.h"
#include "MPU6050.h"
/****************************************************************
* 函数名：Buzzer_Init
* 功能：初始化蜂鸣器引脚 PB5 有源高电平触发
****************************************************************/
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 开GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // PB5 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Buzzer_Off(); // 默认关闭
}

/**********************************************************
 函数名：Buzzer_On
 功能：高电平触发 → 输出高电平，蜂鸣器响
**********************************************************/
void Buzzer_On(void)
{
    // 高电平 → 响
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
    
}

/**********************************************************
 函数名：Buzzer_Off
 功能：输出低电平 → 蜂鸣器不响
**********************************************************/
void Buzzer_Off(void)
{
    // 低电平 → 不响
    
	GPIO_ResetBits(GPIOB, GPIO_Pin_5);
}

void State_Buzzer(void){
	Check_Fall_State();
	if(Fall_Flag == 1){
		Buzzer_On();
	}else{
		Buzzer_Off();
	}
	
}

