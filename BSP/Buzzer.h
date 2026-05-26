#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);  // 初始化蜂鸣器引脚
void Buzzer_On(void);    // 打开蜂鸣器
void Buzzer_Off(void);   // 关闭蜂鸣器
void State_Buzzer(void); //根据Fall_Flag判断是否响
#endif

