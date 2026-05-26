#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"
#include "GPS_USART.h"
#include <string.h>
#include <stdio.h>

// NMEA协议关键语句宏定义
#define GPGGA_HEAD "$GPGGA,"  // 核心定位语句（包含时间、经纬度、海拔）
// GPS数据结构体（完整包含你代码需要的所有成员）
typedef struct {
    float latitude;    // 纬度
    uint8_t lat_dir;   // 北纬N/南纬S
    float longitude;   // 经度
    uint8_t lng_dir;   // 东经E/西经W
    uint8_t hour;      // UTC时
    uint8_t min;       // UTC分
    uint8_t sec;       // UTC秒
    uint8_t fix_state; // 定位状态 1=有效定位
    float altitude;    // 海拔高度
    
    // ========== 原有代码必需成员（补齐！）==========
    uint8_t lat_buf[10];  // 纬度字符串缓存
    uint8_t lng_buf[11];  // 经度字符串缓存
    uint8_t sat_num;      // 卫星数量（解析用，OLED不显示）
    
    // ========== 新增：年月日 ==========
    uint8_t year;      // 年（26=2026）
    uint8_t month;     // 月
    uint8_t day;       // 日
} GPS_TypeDef;
extern GPS_TypeDef GPS_Data;
// 在 GPS.h 里添加（放在结构体外面即可）
extern float last_latitude, last_longitude;
extern uint8_t last_lat_dir, last_lng_dir;
// 全局GPS数据结构体
//extern GPS_Data_TypeDef GPS_Data;

// 函数声明
void GPS_Init(void);
void GPS_Data_Process(void);
u8 GPS_Str_Cmp(u8 *str1, u8 *str2, u8 len);
float GPS_Str_To_Float(u8 *str);
void GPS_Reset_Buffer(void);
void GPS_ShowOnOLED(void);  // 新增：OLED显示GPS数据
void GPS_Config(void);    // GPS模块配置函数
void GPS_PrintDebugInfo(void); //USART1串口查看


// 在原有声明下方添加：
void GPS_SetManualCoords(float lat, float lng, float alt);
void GPS_ClearManualCoords(void);   // 可选，用于切换回GPS自动


#endif

