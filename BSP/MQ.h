#ifndef __MQ_H
#define __MQ_H

#include "stm32f10x.h"
#include "Var.h"
#include "Buzzer.h"
#include "Delay.h"
//void MQ_Init(void);
//void MQ_Get(void);
//void MQ_CheckAlarm(void);
////获取采集数值
//uint16_t MQ5_GetValue(void);
//uint16_t MQ2_GetValue(void);
//// 获取浓度 0~100
//uint16_t MQ2_GetSmoke(void);
//uint16_t MQ5_GetGas(void);

/*---------------------- 外部全局变量声明 -----------------------*/
// 这些变量实际定义在 Var.c 或其他源文件中，此处仅作引用声明
extern uint16_t smoke;      // 烟雾浓度百分比 (0~100%)
extern uint16_t gas;        // 天然气浓度百分比 (0~100%)
extern uint8_t  MQ2_flag;   // 烟雾报警标志 (1=报警，0=正常)
extern uint8_t  MQ5_flag;   // 天然气报警标志
extern uint8_t  buzzer_flag;// 蜂鸣器总开关标志

/*---------------------- 预热相关 -----------------------*/
/**
 * @brief 预热完成标志（全局可读）
 *        0 = 预热未完成，报警功能被屏蔽
 *        1 = 预热已完成，正常执行报警逻辑
 */
extern uint8_t MQ_warmup_done;

/**
 * @brief 启动传感器预热计时
 * @note  应在系统初始化完成、主循环开始前调用一次。
 *        调用后，程序记录当前系统时间，并清零预热完成标志。
 */
void MQ_StartWarmup(void);

/*---------------------- 基本功能函数 -----------------------*/
void     MQ_Init(void);              // 初始化ADC外设
void     MQ_Get(void);               // 读取浓度并更新报警标志（含预热判断）
void     MQ_CheckAlarm(void);        // 综合报警处理（调用MQ_Get + 控制蜂鸣器）
uint16_t MQ2_GetSmoke(void);         // 获取烟雾浓度百分比 (0~100)
uint16_t MQ5_GetGas(void);           // 获取天然气浓度百分比 (0~100)
uint16_t MQ2_GetValue(void);         // 获取MQ2原始ADC平均值 (0~4095)
uint16_t MQ5_GetValue(void);         // 获取MQ5原始ADC平均值 (0~4095)

#endif

