#include "stm32f10x.h"
#include "PWM.h"

  
void Servo_Init(void)		//舵机初始化
{
	
	PWM_Init();
	
}

void Window_Servo_AngleN(float Angle)		//内侧舵机角度传递
{
	
	PWM_SetCompare1N(Angle / 180 * 2000 + 500);
	
}
void Window_Servo_AngleW(float Angle)		//外侧舵机角度传递
{
	
	PWM_SetCompare2W(Angle / 180 * 2000 + 500);
	
}
