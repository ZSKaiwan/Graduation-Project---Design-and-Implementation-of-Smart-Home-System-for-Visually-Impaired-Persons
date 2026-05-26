#include "GPS.h"
#include "OLED.h"  // 新增：包含你的OLED驱动头文件
#include "Delay.h" 

/* 手动坐标覆盖相关 */
static uint8_t  manual_override = 0;   // 1：强制使用手动设定坐标
static float    manual_lat = 0.0f;
static float    manual_lng = 0.0f;
static float    manual_alt = 0.0f;

// 全局变量（和结构体名完全匹配）
GPS_TypeDef GPS_Data = {0};
// 时间备份
uint8_t last_hour = 0, last_min = 0, last_sec = 0;
// 经纬度备份
float last_latitude = 0.0f, last_longitude = 0.0f;
uint8_t last_lat_dir = 'N', last_lng_dir = 'E';
/**
 * @brief  GPS初始化函数
 * @param  无
 * @retval 无
 * @note   初始化GPS串口 + 清空接收缓冲区
 */
void GPS_Init(void)
{
    GPS_USART_Init();    // 初始化GPS串口(PA2/PA3)
    GPS_Reset_Buffer();  // 清空接收缓冲区
}

/**
 * @brief  清空GPS接收缓冲区
 * @param  无
 * @retval 无
 */
void GPS_Reset_Buffer(void)
{
    memset(GPS_RecBuffer, 0, GPS_REC_BUFFER_SIZE);
    GPS_Rec_Cnt = 0;
}

/**
 * @brief  字符串比较函数
 * @param  str1: 字符串1
 * @param  str2: 字符串2
 * @param  len: 比较长度
 * @retval 0:相等 1:不相等
 */
u8 GPS_Str_Cmp(u8 *str1, u8 *str2, u8 len)
{
    while(len--)
    {
        if(*str1 != *str2) return 1;
        str1++;
        str2++;
    }
    return 0;
}

/**
 * @brief  字符串转浮点数
 * @param  str: 数字字符串
 * @retval 转换后的浮点数
 */
float GPS_Str_To_Float(u8 *str)
{
    float num = 0.0;
    u8 point_flag = 0;
    float decimal = 1.0;

    while(*str != '\0' && *str != ',')
    {
        if(*str == '.')
        {
            point_flag = 1;
            str++;
            continue;
        }

        if(!point_flag)
        {
            num = num * 10 + (*str - '0');
        }
        else
        {
            decimal *= 0.1;
            num += (*str - '0') * decimal;
        }
        str++;
    }
    return num;
}

/**
 * @brief  解析GPGGA语句（核心解析函数）
 * @param  buf: NMEA数据缓冲区
 * @retval 无
 * @note   解析$GPGGA语句，提取时间、经纬度、海拔、卫星数
 */
void GPS_Analysis_GPGGA(u8 *buf)
{
    u8 *p = NULL;
    u8 i = 0;
    float temp = 0.0;

    // 1. 提取UTC时间 hhmmss.sss
    p = (u8*)strchr((char *)buf, ',') + 1;
    GPS_Data.hour = (p[0] - '0') * 10 + (p[1] - '0');
    GPS_Data.min  = (p[2] - '0') * 10 + (p[3] - '0');
    GPS_Data.sec  = (p[4] - '0') * 10 + (p[5] - '0');
	if(strstr((char *)buf, "$GPRMC"))
	{
		// 用char数组，匹配sscanf参数
		char date[7] = {0};
		// 强转(char*)消除类型警告
		sscanf((char *)buf, "$GPRMC,%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%[^,]", date);
		
		GPS_Data.day   = (date[0]-'0')*10 + (date[1]-'0');
		GPS_Data.month = (date[2]-'0')*10 + (date[3]-'0');
		GPS_Data.year  = (date[4]-'0')*10 + (date[5]-'0');
	}	
    // 2. 提取纬度 ddmm.mmmm
    p = (u8*)strchr((char *)p, ',') + 1;
    strncpy((char *)GPS_Data.lat_buf, (char *)p, 10);
    // 提取纬度方向 N/S
    p = (u8*)strchr((char *)p, ',') + 1;
    GPS_Data.lat_dir = *p;
    // 纬度转换
    temp = GPS_Str_To_Float(GPS_Data.lat_buf);
    GPS_Data.latitude = (int)(temp / 100) + (temp - (int)(temp / 100) * 100) / 60.0;
    if(GPS_Data.lat_dir == 'S') GPS_Data.latitude = -GPS_Data.latitude;

    // 3. 提取经度 dddmm.mmmm
    p = (u8*)strchr((char *)p, ',') + 1;
    strncpy((char *)GPS_Data.lng_buf, (char *)p, 11);
    // 提取经度方向 E/W
    p = (u8*)strchr((char *)p, ',') + 1;
    GPS_Data.lng_dir = *p;
    // 经度转换
    temp = GPS_Str_To_Float(GPS_Data.lng_buf);
    GPS_Data.longitude = (int)(temp / 100) + (temp - (int)(temp / 100) * 100) / 60.0;
    if(GPS_Data.lng_dir == 'W') GPS_Data.longitude = -GPS_Data.longitude;

    // 4. 提取定位状态
    p = (u8*)strchr((char *)p, ',') + 1;
    GPS_Data.fix_state = *p - '0';

    // 5. 提取卫星数量
    p = (u8*)strchr((char *)p, ',') + 1;
    GPS_Data.sat_num = (p[0] - '0') * 10 + (p[1] - '0');

    // 6. 提取海拔高度
    for(i=0; i<7; i++) p = (u8*)strchr((char *)p, ',') + 1;
    GPS_Data.altitude = GPS_Str_To_Float(p);
}

/**
 * @brief  GPS数据处理主函数
 * @param  无
 * @retval 无
 */
void GPS_Data_Process(void)
{
    if(GPS_Rec_Cnt > 0)
    {
        if(GPS_Str_Cmp(GPS_RecBuffer, (u8 *)GPGGA_HEAD, strlen(GPGGA_HEAD)) == 0)
        {
            GPS_Analysis_GPGGA(GPS_RecBuffer);
        }
        GPS_Reset_Buffer();
    }
		// 时间校验（你已经加过的部分）
	if (GPS_Data.hour > 23 || GPS_Data.min > 59 || GPS_Data.sec > 59)
	{
		GPS_Data.hour = last_hour;
		GPS_Data.min  = last_min;
		GPS_Data.sec  = last_sec;
	}
	else
	{
		last_hour = GPS_Data.hour;
		last_min  = GPS_Data.min;
		last_sec  = GPS_Data.sec;
	}

	// ========== 新增：经纬度合法性校验 ==========
	if (GPS_Data.fix_state == 1) // 仅在有效定位时更新坐标
	{
		last_latitude  = GPS_Data.latitude;
		last_longitude = GPS_Data.longitude;
		last_lat_dir   = GPS_Data.lat_dir;
		last_lng_dir   = GPS_Data.lng_dir;
	}
	else
	{
		// 未定位时，回退到上一次有效坐标（或保持初始值）
		GPS_Data.latitude  = last_latitude;
		GPS_Data.longitude = last_longitude;
		GPS_Data.lat_dir   = last_lat_dir;
		GPS_Data.lng_dir   = last_lng_dir;
	}
	
	
	/* ====== 【新增】手动坐标强制覆盖（放在最后，不破坏原有逻辑） ====== */
    if (manual_override)
    {
        GPS_Data.latitude  = manual_lat;
        GPS_Data.longitude = manual_lng;
        GPS_Data.altitude  = manual_alt;
        GPS_Data.lat_dir   = 'N';   // 手动坐标默认北/东，也可按实际符号设置
        GPS_Data.lng_dir   = 'E';
        // 同步更新备份值，避免将来切回时跳变
        last_latitude  = manual_lat;
        last_longitude = manual_lng;
		GPS_Data.fix_state = 1;       // 强制定位成功
		GPS_Data.sat_num   = 9;      // 固定卫星数（便于识别）
    }

	
	
}

void GPS_ShowOnOLED(void)
{
    // 北京时间转换 UTC+8
    uint8_t bj_hour = GPS_Data.hour + 8;
    if (bj_hour >= 24) bj_hour -= 24;

    // 第1行：CST 时间
    OLED_ShowString(0, 0, "CST:                ", OLED_8X16);
    OLED_Printf(32, 0, OLED_8X16, "%02d:%02d:%02d", bj_hour, GPS_Data.min, GPS_Data.sec);

    // 第2行：纬度
    OLED_ShowString(0, 16, "Lat:                ", OLED_8X16);
    OLED_Printf(32, 16, OLED_8X16, "%.6f%c", GPS_Data.latitude, GPS_Data.lat_dir);

    // 第3行：经度
    OLED_ShowString(0, 32, "Lng:                ", OLED_8X16);
    OLED_Printf(32, 32, OLED_8X16, "%.6f%c", GPS_Data.longitude, GPS_Data.lng_dir);

    // 第4行：重点！
    // 定位成功 → 显示 2026-03-22 50.2M
    // 未定位 → 显示 GPS NO FIX! Sat:x 颗星
    if(GPS_Data.fix_state == 1)
    {
        OLED_ShowString(0, 48, "                    ", OLED_8X16);
        OLED_Printf(0, 48, OLED_8X16, "20%02d-%02d-%02d %.1fM", 
                    GPS_Data.year, GPS_Data.month, GPS_Data.day, GPS_Data.altitude);
    }
    else
    {
        OLED_ShowString(0, 48, "                    ", OLED_8X16);
        OLED_Printf(0, 48, OLED_8X16, "GPS NO FIX!S:%d", GPS_Data.sat_num);
    }

    OLED_Update();
}
/******************************************************************
* @brief   NEO-6M GPS模块配置函数
* @param   无
* @retval  无
* @note   1. 只开启GPGGA语句，关闭其他无用语句，减少数据量
*         2. 设置定位刷新频率1Hz(1次/秒)
*         3. 配置保存到模块FLASH，断电不丢失
*         4. 必须在GPS_USART_Init初始化后调用
******************************************************************/
/******************************************************************
* @brief   【完整版】GPS配置：强制模块为38400波特率 + 全开所有数据(调试)
* @param   无
* @retval  无
* @note   1. 代码强制修改GPS模块波特率为38400
*         2. 开启全部NMEA语句，调试看全数据
*         3. 配置保存到FLASH，永久生效
******************************************************************/
void GPS_Config(void)
{
    u8 i;
    // ==================== 【关键指令】强制GPS模块波特率=38400 ====================
    u8 CMD_SET_38400[] = {0xB5,0x62,0x06,0x00,0x14,0x00,0x01,0x00,0x00,0x00,0xD0,0x08,0x00,0x00,0x00,0x96,0x00,0x00,0x07,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x96,0xA4};
    
    // 开启GPS所有NMEA数据（调试专用，能看搜星/定位全信息）
    u8 CMD_OPEN_ALL[] = {0xB5,0x62,0x06,0x00,0x14,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x21,0x50};
    
    // 保存配置（断电不丢失）
    u8 CMD_SAVE[] = {0xB5,0x62,0x06,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x0E,0x34};

    // 1. 发指令：GPS模块强制改为38400波特率
    for(i=0; i<sizeof(CMD_SET_38400); i++)  GPS_USART_SendByte(CMD_SET_38400[i]);
    Delay_ms(100);
    
    // 2. 发指令：开启所有GPS数据（调试用）
    for(i=0; i<sizeof(CMD_OPEN_ALL); i++)    GPS_USART_SendByte(CMD_OPEN_ALL[i]);
    Delay_ms(100);
    
    // 3. 发指令：永久保存
    for(i=0; i<sizeof(CMD_SAVE); i++)       GPS_USART_SendByte(CMD_SAVE[i]);
    Delay_ms(100);
}



/**
 * @brief  设置手动覆盖坐标（云端下发时调用）
 * @param  lat 纬度（度）
 * @param  lng 经度（度）
 * @param  alt 海拔（米）
 */
void GPS_SetManualCoords(float lat, float lng, float alt)
{
    manual_lat = lat;
    manual_lng = lng;
    manual_alt = alt;
    manual_override = 1;
	
	// 【新增】直接模拟定位成功
    GPS_Data.fix_state = 1;
    GPS_Data.sat_num  = 9;       // 通过云平台下发时，卫星数显示99作为标记
}

/**
 * @brief  清除手动覆盖，恢复 GPS 自动坐标
 */
void GPS_ClearManualCoords(void)
{
    manual_override = 0;
}

