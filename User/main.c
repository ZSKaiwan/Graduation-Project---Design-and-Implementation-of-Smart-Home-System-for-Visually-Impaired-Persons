#include "stm32f10x.h"
#include "Var.h"
#include "OLED.h"
#include "MPU6050.h"
#include <stdio.h>
#include <stdlib.h>
#include "Delay.h"
#include "Buzzer.h"
#include "GPS_USART.h"
#include "GPS.h"
#include "ESP8266.h"
#include "MQTT.h"

/**
 * @brief 属性设置回调函数（处理云端下发的指令）
 * @param payload JSON字符串，如 {"id":"xxx","params":{"Buzzer":true}}
 * @note 该函数在MQTT接收线程（主循环中的ESP8266_ProcessIPD）中调用
 */
void on_property_set(char *payload)
{
    // 1. 提取msg_id，用于后续回复（OneNET要求）
    char *id_start = strstr(payload, "\"id\":\"");
    if (id_start) {
        id_start += 6;  // 跳过 "id":" 
        char *id_end = strchr(id_start, '"');
        if (id_end) {
            int len = id_end - id_start;
            if (len > 0 && len < (int)sizeof(reply_msg_id)) {
                strncpy(reply_msg_id, id_start, len);
                reply_msg_id[len] = '\0';
                pending_property_reply = 1;   // 标记有待发送的响应
            }
        }
    }
    
    // 2. 解析Buzzer控制指令
    char *buzzer_pos = strstr(payload, "\"Buzzer\"");
    if (buzzer_pos) {
        char *colon = strchr(buzzer_pos, ':');
        if (colon) {
            colon++;
            while (*colon == ' ' || *colon == '\t') colon++; // 跳过空白字符
            if (strncmp(colon, "true", 4) == 0 || *colon == '1')
                Buzzer_On();   // 云端开启蜂鸣器
            else if (strncmp(colon, "false", 5) == 0 || *colon == '0')
                Buzzer_Off();  // 云端关闭蜂鸣器
        }
    }
	
	
	
	// 3. 解析 GPS 坐标手动设置
    char *gps_pos = strstr(payload, "\"gps\"");
    if (gps_pos) {
        char *value_pos = strstr(gps_pos, "\"value\"");
        if (value_pos) {
            float new_lat = 0.0f, new_lng = 0.0f, new_alt = 0.0f;
            uint8_t got_lat = 0, got_lng = 0;

            // 解析 lat
            char *lat_ptr = strstr(value_pos, "\"lat\"");
            if (lat_ptr) {
                lat_ptr = strchr(lat_ptr, ':');
                if (lat_ptr) {
                    new_lat = atof(lat_ptr + 1);
                    got_lat = 1;
                }
            }
            // 解析 lng
            char *lng_ptr = strstr(value_pos, "\"lng\"");
            if (lng_ptr) {
                lng_ptr = strchr(lng_ptr, ':');
                if (lng_ptr) {
                    new_lng = atof(lng_ptr + 1);
                    got_lng = 1;
                }
            }
            // 解析 alt（可选）
            char *alt_ptr = strstr(value_pos, "\"alt\"");
            if (alt_ptr) {
                alt_ptr = strchr(alt_ptr, ':');
                if (alt_ptr) {
                    new_alt = atof(alt_ptr + 1);
                }
            }

            if (got_lat && got_lng) {
                GPS_SetManualCoords(new_lat, new_lng, new_alt);
            }
        }
    }
	
	
	
}


// 整体重连函数
void ReconnectNetwork(void)
{
    ESP8266_ExitTransparent();          // 退出透传
    Delay_ms(500);
    ESP8266_Init();
    while (ESP8266_ConnectWiFi("iQOO10", "16627891336") != 0) {
        Delay_ms(5000);
    }
    while (ESP8266_ConnectMQTTServer() != 0) {
        Delay_ms(5000);
    }

    MQTT_Init();
    MQTT_SetPropertyCallback(on_property_set);
}

int main(void)
{
	SystemInit();  

	Delay_Init();	
	DWT_Init();	

	Buzzer_Init();

    MPU6050_Init();

    GPS_Init();
    GPS_Config();

    ESP8266_Init();

    // ==== 连接 WiFi====
    while (ESP8266_ConnectWiFi("iQOO10", "16627891336") != 0) {
        Delay_ms(3000);
    }

    // ==== 连接 OneNET MQTT 服务器 ====
    while (ESP8266_ConnectMQTTServer() != 0) {
        Delay_ms(5000);
    }
    MQTT_Init();
    MQTT_SetPropertyCallback(on_property_set);


    /*-------------------------- 主循环变量 --------------------------*/
    uint32_t last_gps_report = 0;


	Buzzer_On();Delay_ms(50);Buzzer_Off();
	
	GPS_SetManualCoords(33.013333f, 114.002222f, 50.0f);
    while (1)
    {
        ESP8266_ProcessIPD();
		MQTT_Process();
		
		MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
		Calculate_G_Value();
		State_Buzzer();         // 本地蜂鸣器控制


		GPS_Data_Process();         // 解析 GPS 数据（更新 GPS_Data 结构体）

		// ==== 无条件定时上报 GPS 位置（每 5 秒） ====
		if (Delay_GetMs() - last_gps_report >= 1000) {
			if (MQTT_IsConnected()) {
				MQTT_ReportGPS(GPS_Data.latitude, GPS_Data.longitude, GPS_Data.altitude);
			}
			last_gps_report = Delay_GetMs();
		}

		// ==== 无条件定时上报跌倒状态（每 5 秒） ====
		static uint32_t last_fall_report = 0;
		if (Delay_GetMs() - last_fall_report >= 500) {
			if (MQTT_IsConnected()) {
				MQTT_ReportFall(Fall_Flag);
			}
			last_fall_report = Delay_GetMs();
		}

		GPS_ShowOnOLED();   // 显示 GPS 信息（如果 OLED 启用）

		Delay_ms(1000);     // 主循环延时 1 秒，降低 CPU 占用，同时影响定时精度
    }
}

