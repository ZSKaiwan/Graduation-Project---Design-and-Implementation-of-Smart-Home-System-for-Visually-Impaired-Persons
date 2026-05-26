#include "stm32f10x.h"   
#include "Delay.h"
#include "Var.h"
#include "Servo.h"

void Action_Window(){
	Delay_ms(60);
	if(Window_Mode == 0){									//当关闭时
		
				WindowN_Value = 0;						//改变内侧舵机compareֵ
				Window_Servo_AngleN(WindowN_Value);	
	}else if(Window_Mode == 1){								//当开启时
		
				WindowN_Value = 120;						//改变内侧舵机compareֵ
				Window_Servo_AngleN(WindowN_Value);				
			
	}
	Delay_ms(60);
}

