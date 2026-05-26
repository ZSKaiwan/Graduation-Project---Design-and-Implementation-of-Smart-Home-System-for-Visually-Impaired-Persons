#ifndef __MQTT_H
#define __MQTT_H

#include "stm32f10x.h"
#include <stdint.h>

/*-------------------------- OneNET平台配置（从机专用）--------------------------*/
#define PRODUCT_ID      "iSo9o2qsGz"           // 产品ID，与主机保持一致
#define DEVICE_NAME     "Slave"         // 从机设备名称，需在OneNET控制台创建
#define PASSWORD        "version=2018-10-31&res=products%2FiSo9o2qsGz%2Fdevices%2FSlave&et=1999999999&method=md5&sign=mFseKKKj3%2FUVU4rOh6lbhg%3D%3D"     // 设备密钥生成的Token，替换为实际值

/*-------------------------- MQTT主题宏定义 --------------------------*/
#define TOPIC_PROPERTY_POST   "$sys/%s/%s/thing/property/post"   // 属性上报主题
#define TOPIC_PROPERTY_SET    "$sys/%s/%s/thing/property/set"    // 属性设置订阅主题

#define MQTT_KEEPALIVE_INTERVAL 50   // 心跳间隔（秒），必须小于CONNECT报文中的KeepAlive值（100秒）

/*-------------------------- 全局标志（用于延迟发送，与主机一致）--------------------------*/
extern volatile uint8_t  pending_puback;       // 有待发送的PUBACK标志
extern volatile uint16_t pending_packet_id;    // 待回复的Packet ID
extern volatile uint8_t  pending_property_reply; // 有待发送的属性设置响应标志
extern char              reply_msg_id[16];     // 属性设置响应中的msg_id

/*-------------------------- 函数声明 --------------------------*/
void MQTT_Init(void);                          // MQTT初始化（发送CONNECT）
void MQTT_ReportGPS(float lat, float lng, float alt);   // 上报GPS位置
void MQTT_ReportFall(uint8_t fall_flag);               // 上报跌倒状态
void MQTT_Process(void);                       // MQTT主处理函数（心跳、重连、订阅）
uint8_t MQTT_IsConnected(void);                // 查询MQTT连接状态
void MQTT_ReceiveByte(uint8_t byte);           // 接收字节处理（状态机）
void MQTT_SetPropertyCallback(void (*callback)(char *payload)); // 注册属性设置回调

uint16_t build_publish(uint8_t *pkt, const char *topic, const char *payload); // 构建PUBLISH报文
uint16_t build_subscribe(uint8_t *pkt);        // 构建SUBSCRIBE报文（属性设置主题）

#endif

