#include "stm32f10x.h"
#include "Var.h"

// 全局变量定义

uint8_t Fall_Flag = 0;	// 跌倒标志位 1=跌倒 0=正常

uint8_t ID;								//定义用于存放ID号的变量
int16_t AX, AY, AZ, GX, GY, GZ;			//定义用于存放各个数据的变量

// 计算后重力加速度变量
// 重力加速度 → 直接赋初值 1.0f（正常重力值）
float Ax = 0.0f;
float Ay = 0.0f;
float Az = 1.0f;
float g_value = 1.0f;	
