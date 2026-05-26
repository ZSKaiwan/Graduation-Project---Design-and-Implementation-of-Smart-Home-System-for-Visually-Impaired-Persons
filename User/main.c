#include "stm32f10x.h"
#include "OLED.h"
#include "Servo.h"
#include "Action.h"
#include "PWM.h"
#include "Var.h"
#include "Voice.h"
#include <stdio.h>
#include <stdlib.h>
#include "Delay.h"
#include "dht11.h"
#include "Usart.h"
#include "MQTT.h"
#include "ESP8266.h"
#include "MQ.h"
#include "Buzzer.h"
#include <string.h>

uint8_t subscribe_packet[128];//定义subscribe报文缓冲区
uint16_t subscribe_len=0;    //构建subscribe报文 
uint8_t subscribed = 0;

/* ==========全局标志，用于延迟发送 PUBACK 和属性响应 ========== */
volatile uint8_t  pending_puback = 0;      // 是否有待发送的 PUBACK
volatile uint16_t pending_packet_id = 0;   // 待回复的 Packet ID

volatile uint8_t  pending_property_reply = 0; // 是否有待发送的属性响应
char              reply_msg_id[16] = {0};     // 响应中的 msg_id
/* ===================================================================== */

// ==== 新增：跌倒报警标志位（用于非阻塞响铃） ====
volatile uint8_t fall_alert_flag = 0;   // 1 = 需要触发蜂鸣器报警
// ===============================================
// ==== 新增：跌倒报警持续标志 ====
volatile uint8_t fall_buzzer_active = 0;   // 1 = 因跌倒报警而开启蜂鸣器，需云端指令关闭
// ==============================
//volatile uint8_t fall_alert_flag = 0;       // 触发标志（单次有效）
//volatile uint8_t fall_buzzer_active = 0;    // 持续报警状态标志
/* 函数声明 */
void ProcessSlaveData(uint8_t link_id, uint8_t *data, uint16_t len);


/**
 * @brief 属性设置回调函数（处理云端下发的指令）
 * @param payload JSON字符串，例如 {"params":{"Servo":true}}
 */
void on_property_set(char *payload)
{
    Buzzer_On(); Delay_ms(50); Buzzer_Off();  // 调试指示

    char *id_start = strstr(payload, "\"id\":\"");
    if (id_start) {
        id_start += 6;
        char *id_end = strchr(id_start, '"');
        if (id_end) {
			 int len = id_end - id_start;
            // ========== 确保不超过缓冲区，并过滤异常长度 ==========
            if (len > 0 && len < (int)sizeof(reply_msg_id)) {
                strncpy(reply_msg_id, id_start, len);
                reply_msg_id[len] = '\0';
                pending_property_reply = 1;
            } else {
                // id 格式异常，放弃回复（避免无效响应）
                pending_property_reply = 0;
            }
        }
    }

    // 解析舵机指令（兼容 "Servo":true 和 "Servo":{"value":true} 两种格式）
    char *servo_pos = strstr(payload, "\"Servo\"");
    if (servo_pos) {
        // 查找紧跟在 "Servo" 后面的冒号，然后判断值
        char *colon = strchr(servo_pos, ':');
        if (colon) {
            colon++;
            while (*colon == ' ' || *colon == '\t') colon++;
            if (strncmp(colon, "true", 4) == 0 || *colon == '1') {
                Window_Mode = 1;
                Servo = 1;
            } else if (strncmp(colon, "false", 5) == 0 || *colon == '0') {
                Window_Mode = 0;
                Servo = 0;
            }
        }
    }
	
	// ==== 解析蜂鸣器控制指令（云端主动开关） ====
		char *buzzer_pos = strstr(payload, "\"Buzzer\"");
		if (buzzer_pos) {
			char *colon = strchr(buzzer_pos, ':');
			if (colon) {
				colon++;
				while (*colon == ' ' || *colon == '\t') colon++;
				if (strncmp(colon, "true", 4) == 0 || *colon == '1') {
					Buzzer_On();
					fall_buzzer_active = 1;
					// 云端主动开启时，不需要报警标志
				} else if (strncmp(colon, "false", 5) == 0 || *colon == '0') {
					Buzzer_Off();
					// ==== 新增：关闭因报警触发的蜂鸣器标志 ====
					fall_buzzer_active = 0;
					// ======================================
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

int main()
{	
	SystemInit();  

	Delay_Init();	
	DWT_Init();	

    OLED_Init();
    OLED_Clear();
    OLED_ShowChinese(15,24,"欢迎，请稍等！");
    OLED_Update();
    Servo_Init();

	Voice_Init();
    MQ_Init();
	MQ_StartWarmup();//启动预热计时
    DHT11_Init();
    Buzzer_Init();
	
	uint32_t last_report = Delay_GetMs();

	// 主机初始化
    ESP8266_Init(); 						// 复位、AT测试、设置模式
	
	   // ====== WiFi连接增加返回值检查 ======
    while (ESP8266_ConnectWiFi("iQOO10", "16627891336") != 0) {
        Delay_ms(3000);
    }
 
    // 5. MQTT初始化（发送CONNECT和SUBSCRIBE报文）
    MQTT_Init();
	
    MQTT_SetPropertyCallback(on_property_set);   // 注册属性设置回调
	OLED_Clear();
	OLED_Update();

    while(1)
    {
		ESP8266_ProcessIPD();   // 处理从 OneNET 收到的指令
		
		// ==== 处理跌倒报警触发标志 ====
		if (fall_alert_flag) {
			fall_buzzer_active = 1;   // 标记为持续报警状态
			fall_alert_flag = 0;      // 清除单次触发标志
		}
		// ============================

		// ==== 持续报警控制（根据 fall_buzzer_active 维持蜂鸣器状态） ====
		if (fall_buzzer_active) {
			Buzzer_On();              // 只要报警标志有效，就保持蜂鸣器开启
		}
		// 注意：当云端下发 Buzzer:false 时，on_property_set 中会将 fall_buzzer_active 清零，
		//       下一循环本判断不再执行 Buzzer_On()，蜂鸣器随之关闭。
		// ==============================================================
		
		/* ========== 延迟发送 PUBACK（从 MQTT_ReceiveByte 移到这里） ========== */
		if (pending_puback) {
			uint8_t puback[4];
			puback[0] = 0x40;
			puback[1] = 0x02;
			puback[2] = (pending_packet_id >> 8) & 0xFF;
			puback[3] = pending_packet_id & 0xFF;
			if (ESP8266_SendData(puback, 4) == 0) {
				pending_puback = 0;   // 发送成功，清除标志
			}
		}
		/* ========================================================================= */

		/* ========== 延迟发送属性设置响应 ========== */
		if (pending_property_reply) {
			// 快速构建并发送响应
			const char *reply_topic = "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set_reply";
			char reply_payload[64];  // 足够容纳 {"id":"xxxx","code":200,"msg":"success"}
			sprintf(reply_payload, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", reply_msg_id);
			
			uint8_t pkt_buf[128];
			uint16_t pkt_len = build_publish(pkt_buf, reply_topic, reply_payload);
			int8_t ret = ESP8266_SendData(pkt_buf, pkt_len);
			
			if (ret == 0) {
				pending_property_reply = 0;  // 发送成功，清除标志
			} else {
				// 发送失败，保留标志等待下次重试（但不阻塞其他任务）
				// 增加一个简单重试计数，避免无限循环
				static uint8_t reply_fail_cnt = 0;
				reply_fail_cnt++;
				if (reply_fail_cnt >= 3) {
					pending_property_reply = 0;  // 放弃
					reply_fail_cnt = 0;
				}
			}
		}
		/* ============================================= */
		
		
		
		if (!subscribed && MQTT_IsConnected()) {
			subscribe_len = build_subscribe(subscribe_packet);
			if (ESP8266_SendData(subscribe_packet, subscribe_len) == 0) {
				subscribed = 1;
				Start_flag = 1;
				Delay_ms(500);   // 等待模块处理
			} else {
				// 发送失败，稍后重试（可能是连接暂时不可用）
				Delay_ms(1000);
			}
		}
		MQTT_Process();
        DHTcollect_data();
        OLED_Interface();
        Action_Window();
        MQ_CheckAlarm();
		if (Delay_GetMs() - last_report >= 3000) {
			if (MQTT_IsConnected()) {
				MQTT_ReportAllSensors();
			}
			last_report = Delay_GetMs();
		}
		

		
		
        Delay_ms(10);

    }

}




