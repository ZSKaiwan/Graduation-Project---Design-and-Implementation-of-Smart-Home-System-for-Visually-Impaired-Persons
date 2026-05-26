#ifndef __MQTT_H
#define __MQTT_H

#include "stm32f10x.h"
#include <stdint.h>

/*-------------------------- OneNET配置（请根据实际修改）--------------------------*/
#define PRODUCT_ID      "iSo9o2qsGz"      // 产品ID
#define DEVICE_NAME     "Test"            // 设备名称（ClientID）
#define PASSWORD        "version=2018-10-31&res=products%2FiSo9o2qsGz%2Fdevices%2FTest&et=1999999999&method=md5&sign=HJAvvTWUCa7Fz03qqRDc0w%3D%3D"

 //MQTT主题
#define TOPIC_PROPERTY_POST   "$sys/%s/%s/thing/property/post"   // 属性上报
#define TOPIC_PROPERTY_SET    "$sys/%s/%s/thing/property/set"    // 属性设置（订阅）



 //心跳间隔（秒），需小于KeepAlive（100秒）
#define MQTT_KEEPALIVE_INTERVAL 50
/* 声明供 main.c 使用的延迟发送标志 */
extern volatile uint8_t  pending_puback;
extern volatile uint16_t pending_packet_id;
extern volatile uint8_t  pending_property_reply;
extern char              reply_msg_id[16];
/*-------------------------- 函数声明 --------------------------*/
/**
 * @brief 初始化MQTT（发送CONNECT和SUBSCRIBE）
 */
void MQTT_Init(void);

/**
 * @brief 上报数据
 * @param float temperature, float humidity,uint8_t servo_switch,
                 float mq2_gas1, float mq2_gas2, float mq2_gas3, float mq2_gas4, 
				float mq2_gas5
 */
void MQTT_ReportAllSensors(void);

/**
 * @brief 心跳发送（需周期性调用）
 */
void MQTT_KeepAlive(void);

/**
 * @brief MQTT主处理（接收消息解析、心跳）
 */
void MQTT_Process(void);

/**
 * @brief 注册属性设置回调函数
 * @param callback 参数为payload字符串
 */
void MQTT_SetPropertyCallback(void (*callback)(char *payload));

/**
 * @brief 从串口接收一个字节（在USART3中断中调用）
 */
uint8_t MQTT_IsConnected(void);
void MQTT_ReceiveByte(uint8_t byte);
uint16_t build_subscribe(uint8_t *pkt);
uint16_t build_publish(uint8_t *pkt, const char *topic, const char *payload);


#endif




