/**
 * @file MQTT.c
 * @brief 从机MQTT协议实现（非透传模式，与主机驱动兼容）
 * @note 支持CONNECT/PUBLISH/SUBSCRIBE/PINGREQ，处理属性设置下发，维持心跳
 */

#include "MQTT.h"
#include "ESP8266.h"
#include "Delay.h"
#include "GPS.h"
#include <string.h>
#include <stdio.h>

/*-------------------------- 内部发送/接收缓冲区 --------------------------*/
static uint8_t tx_buf[512];          // 发送缓冲区，用于构建MQTT报文
static uint8_t rx_buf[512];          // 接收缓冲区，存储正在接收的MQTT报文载荷
static uint32_t last_heartbeat = 0;  // 上次心跳发送时间（毫秒）
static void (*property_cb)(char *payload) = NULL; // 属性设置回调函数指针

/*-------------------------- 全局标志定义（供外部使用）--------------------------*/
volatile uint8_t  pending_puback = 0;
volatile uint16_t pending_packet_id = 0;
volatile uint8_t  pending_property_reply = 0;
char              reply_msg_id[16] = {0};

static uint8_t subscribed = 0;        // 是否已订阅属性设置主题

/*-------------------------- MQTT接收状态机 --------------------------*/
typedef enum {
    MQTT_RX_IDLE,       // 空闲，等待固定头
    MQTT_RX_REM_LEN,    // 正在读取剩余长度（多字节编码）
    MQTT_RX_PAYLOAD     // 正在读取载荷数据
} MqttRxState;

static MqttRxState rx_state = MQTT_RX_IDLE;
static uint8_t  fixed_header;        // 报文固定头（第一字节）
static uint32_t remaining_len = 0;   // 解码后的剩余长度
static uint8_t  rem_len_bytes = 0;   // 已读取的剩余长度字节数
static uint16_t payload_index = 0;   // 当前载荷写入位置

static uint8_t connack_received = 0; // 是否收到CONNACK
static uint8_t ping_missed = 0;      // 连续心跳无响应计数

/*-------------------------- 编码剩余长度（MQTT可变字节）--------------------------*/
uint8_t encode_remaining_length(uint8_t *buf, uint32_t len)
{
    uint8_t pos = 0;
    do {
        uint8_t digit = len % 128;
        len /= 128;
        if (len > 0) digit |= 0x80;   // 最高位置1表示后续还有字节
        buf[pos++] = digit;
    } while (len > 0 && pos < 4);
    return pos;  // 返回编码后的字节数
}

/*-------------------------- 构建 CONNECT 报文 --------------------------*/
static uint16_t build_connect(uint8_t *pkt)
{
    uint16_t pos = 0;
    uint32_t rem_len;
    uint8_t len_buf[4], len_bytes;
    
    pkt[pos++] = 0x10;  // 固定头：CONNECT
    
    // 计算剩余长度：协议名(2+4) + 协议级别(1) + 连接标志(1) + KeepAlive(2) + ClientID(2+len) + Username(2+len) + Password(2+len)
    rem_len = 10 + 2 + strlen(DEVICE_NAME) + 2 + strlen(PRODUCT_ID) + 2 + strlen(PASSWORD);
    len_bytes = encode_remaining_length(len_buf, rem_len);
    memcpy(&pkt[pos], len_buf, len_bytes); pos += len_bytes;
    
    // 协议名 "MQTT"
    pkt[pos++] = 0x00; pkt[pos++] = 0x04;
    memcpy(&pkt[pos], "MQTT", 4); pos += 4;
    pkt[pos++] = 0x04;               // 协议级别：MQTT 3.1.1
    pkt[pos++] = 0xC2;               // 连接标志：Clean Session，包含用户名和密码
    pkt[pos++] = 0x00; pkt[pos++] = 0x64; // Keep Alive 100秒
    
    // Client Identifier（设备名称）
    pkt[pos++] = (strlen(DEVICE_NAME) >> 8) & 0xFF;
    pkt[pos++] = strlen(DEVICE_NAME) & 0xFF;
    memcpy(&pkt[pos], DEVICE_NAME, strlen(DEVICE_NAME)); pos += strlen(DEVICE_NAME);
    
    // User Name（产品ID）
    pkt[pos++] = (strlen(PRODUCT_ID) >> 8) & 0xFF;
    pkt[pos++] = strlen(PRODUCT_ID) & 0xFF;
    memcpy(&pkt[pos], PRODUCT_ID, strlen(PRODUCT_ID)); pos += strlen(PRODUCT_ID);
    
    // Password（Token）
    pkt[pos++] = (strlen(PASSWORD) >> 8) & 0xFF;
    pkt[pos++] = strlen(PASSWORD) & 0xFF;
    memcpy(&pkt[pos], PASSWORD, strlen(PASSWORD)); pos += strlen(PASSWORD);
    
    return pos;
}

/*-------------------------- 构建 PUBLISH 报文 (QoS=0) --------------------------*/
uint16_t build_publish(uint8_t *pkt, const char *topic, const char *payload)
{
    uint16_t pos = 0;
    uint32_t rem_len = 2 + strlen(topic) + strlen(payload); // 主题长度字段(2) + 主题内容 + 载荷
    uint8_t len_buf[4];
    uint8_t len_bytes = encode_remaining_length(len_buf, rem_len);
    
    pkt[pos++] = 0x30;  // 固定头：PUBLISH, QoS=0
    memcpy(&pkt[pos], len_buf, len_bytes); pos += len_bytes;
    
    // 主题长度（大端序）
    pkt[pos++] = (strlen(topic) >> 8) & 0xFF;
    pkt[pos++] = strlen(topic) & 0xFF;
    memcpy(&pkt[pos], topic, strlen(topic)); pos += strlen(topic);
    
    // 有效载荷
    memcpy(&pkt[pos], payload, strlen(payload)); pos += strlen(payload);
    return pos;
}

/*-------------------------- 构建 SUBSCRIBE 报文（订阅属性设置主题）--------------------------*/
uint16_t build_subscribe(uint8_t *pkt)
{
    uint16_t pos = 0;
    uint32_t rem_len;
    uint8_t len_buf[4], len_bytes;
    const char *topic = "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set";
    uint16_t topic_len = strlen(topic);
    
    pkt[pos++] = 0x82;  // 固定头：SUBSCRIBE, QoS=1
    
    // 剩余长度 = Packet ID(2) + 主题长度(2) + 主题内容 + QoS(1)
    rem_len = 2 + 2 + topic_len + 1;
    len_bytes = encode_remaining_length(len_buf, rem_len);
    memcpy(&pkt[pos], len_buf, len_bytes); pos += len_bytes;
    
    // Packet Identifier
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    
    // 主题长度及内容
    pkt[pos++] = (topic_len >> 8) & 0xFF;
    pkt[pos++] = topic_len & 0xFF;
    memcpy(&pkt[pos], topic, topic_len); pos += topic_len;
    
    // QoS要求（0）
    pkt[pos++] = 0x00;
    
    return pos;
}

/*-------------------------- 构建 PINGREQ 心跳请求报文 --------------------------*/
static uint16_t build_pingreq(uint8_t *pkt)
{
    pkt[0] = 0xC0;  // 固定头：PINGREQ
    pkt[1] = 0x00;  // 剩余长度0
    return 2;
}

/*-------------------------- 接收一个字节（状态机处理）--------------------------*/
void MQTT_ReceiveByte(uint8_t byte)
{
    switch (rx_state) {
        case MQTT_RX_IDLE:
            fixed_header = byte;                     // 记录固定头
            rx_state = MQTT_RX_REM_LEN;
            remaining_len = 0;
            rem_len_bytes = 0;
            payload_index = 0;
            if (payload_index < sizeof(rx_buf))
                rx_buf[payload_index++] = byte;      // 存入缓冲区供后续解析
            break;
            
        case MQTT_RX_REM_LEN:
            remaining_len |= (byte & 0x7F) << (7 * rem_len_bytes);
            rem_len_bytes++;
            if (payload_index < sizeof(rx_buf))
                rx_buf[payload_index++] = byte;
            if ((byte & 0x80) == 0) {                // 剩余长度读取完毕
                if ((fixed_header & 0xF0) == 0x20)   // CONNACK报文
                    connack_received = 1;
                if (remaining_len > 0) {
                    rx_state = MQTT_RX_PAYLOAD;      // 有载荷，进入载荷接收
                } else {
                    if ((fixed_header & 0xF0) == 0xD0) // PINGRESP
                        ping_missed = 0;
                    rx_state = MQTT_RX_IDLE;
                }
            } else if (rem_len_bytes >= 4) {          // 剩余长度字段超过4字节，非法
                rx_state = MQTT_RX_IDLE;
            }
            break;
            
        case MQTT_RX_PAYLOAD:
            if (payload_index < sizeof(rx_buf)) {
                rx_buf[payload_index++] = byte;
                // 计算已接收的载荷长度（跳过固定头和剩余长度字段）
                uint16_t header_and_rem_len_bytes = 1 + rem_len_bytes;
                uint16_t received_payload = payload_index - header_and_rem_len_bytes;
                if (received_payload >= remaining_len) {
                    // 完整报文接收完毕
                    if ((fixed_header & 0xF0) == 0x30) { // PUBLISH报文
                        uint8_t *ptr = rx_buf + header_and_rem_len_bytes;
                        uint16_t topic_len = (ptr[0] << 8) | ptr[1];
                        ptr += 2;
                        ptr += topic_len;                // 跳过主题内容
                        
                        uint8_t qos = (fixed_header & 0x06) >> 1;
                        uint16_t packet_id = 0;
                        if (qos > 0) {
                            packet_id = (ptr[0] << 8) | ptr[1];
                            ptr += 2;
                            // 记录待发送PUBACK
                            pending_puback = 1;
                            pending_packet_id = packet_id;
                        }
                        
                        // 计算有效载荷长度并调用回调
                        uint32_t payload_len = remaining_len - (2 + topic_len + (qos > 0 ? 2 : 0));
                        if (payload_len > 0 && property_cb) {
                            char payload_str[256];
                            if (payload_len < sizeof(payload_str)) {
                                memcpy(payload_str, ptr, payload_len);
                                payload_str[payload_len] = '\0';
                                property_cb(payload_str);   // 调用属性设置回调
                            }
                        }
                    } else if ((fixed_header & 0xF0) == 0x20) {
                        connack_received = 1;               // CONNACK处理
                    }
                    rx_state = MQTT_RX_IDLE;
                }
            } else {
                rx_state = MQTT_RX_IDLE;   // 缓冲区溢出，重置
            }
            break;
    }
}

/*-------------------------- 等待 CONNACK 响应（阻塞）--------------------------*/
static uint8_t wait_connack(uint32_t timeout_ms)
{
    uint32_t start = Delay_GetMs();
    connack_received = 0;
    while (Delay_GetMs() - start < timeout_ms) {
        if (connack_received) {
            connack_received = 0;
            return 1;
        }
        Delay_ms(10);
    }
    return 0;
}

/*-------------------------- 公开函数实现 --------------------------*/

/**
 * @brief MQTT初始化：发送CONNECT并等待CONNACK
 */
void MQTT_Init(void)
{
    uint16_t len = build_connect(tx_buf);
    ESP8266_SendData(tx_buf, len);
    wait_connack(5000);   // 等待5秒内收到CONNACK
    last_heartbeat = Delay_GetMs();
    ping_missed = 0;
    subscribed = 0;
}

/**
 * @brief 上报GPS位置（物模型属性）
 * @param lat 纬度（度）
 * @param lng 经度（度）
 * @param alt 海拔（米）
 */
void MQTT_ReportGPS(float lat, float lng, float alt)
{
    char topic[128];
    char payload[256];
    sprintf(topic, TOPIC_PROPERTY_POST, PRODUCT_ID, DEVICE_NAME);
    sprintf(payload,
		"{\"id\":\"123\",\"params\":{"
		"\"gps\":{\"value\":{"
		"\"lat\":%.6f,\"lng\":%.6f,\"alt\":%.1f,"
		"\"fix\":%s,\"sat\":%d"        // fix 改为字符串 "true"/"false"
		"}}"
		"}}",
		lat, lng, alt,
		GPS_Data.fix_state ? "true" : "false",   // 转换为布尔值
		GPS_Data.sat_num);
    uint16_t len = build_publish(tx_buf, topic, payload);
    ESP8266_SendData(tx_buf, len);
}

/**
 * @brief 上报跌倒状态（布尔型属性）
 * @param fall_flag 1=跌倒，0=正常
 */
void MQTT_ReportFall(uint8_t fall_flag)
{
    char topic[128];
    char payload[128];
    sprintf(topic, TOPIC_PROPERTY_POST, PRODUCT_ID, DEVICE_NAME);
    sprintf(payload,
        "{\"id\":\"123\",\"params\":{"
        "\"fall\":{\"value\":%s}"
        "}}",
        fall_flag ? "true" : "false");
    uint16_t len = build_publish(tx_buf, topic, payload);
    ESP8266_SendData(tx_buf, len);
}

/**
 * @brief MQTT主处理函数：维持心跳、处理重连、自动订阅、发送延迟的PUBACK和响应
 */
void MQTT_Process(void)
{
    uint32_t now = Delay_GetMs();
    
    // 1. 发送待处理的PUBACK
    if (pending_puback) {
        uint8_t puback[4] = {0x40, 0x02, (pending_packet_id >> 8) & 0xFF, pending_packet_id & 0xFF};
        if (ESP8266_SendData(puback, 4) == 0)
            pending_puback = 0;
    }
    
    // 2. 发送待处理的属性设置响应
    if (pending_property_reply) {
        const char *reply_topic = "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set_reply";
        char reply_payload[64];
        sprintf(reply_payload, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", reply_msg_id);
        uint8_t pkt_buf[128];
        uint16_t pkt_len = build_publish(pkt_buf, reply_topic, reply_payload);
        if (ESP8266_SendData(pkt_buf, pkt_len) == 0)
            pending_property_reply = 0;
    }
    
    // 3. 检查TCP连接状态
    if (!ESP8266_IsConnected()) {
        ping_missed = 0;
        subscribed = 0;
        ESP8266_ConnectMQTTServer();        // 重新建立TCP连接
        Delay_ms(1000);
        uint16_t len = build_connect(tx_buf);
        ESP8266_SendData(tx_buf, len);      // 重新发送CONNECT
        last_heartbeat = now;
        return;
    }
    
    // 4. 连接成功但未订阅时，自动订阅属性设置主题
    if (connack_received && !subscribed) {
        uint16_t sub_len = build_subscribe(tx_buf);
        if (ESP8266_SendData(tx_buf, sub_len) == 0)
            subscribed = 1;
    }
    
    // 5. 心跳发送（带推迟机制，避免与属性响应冲突）
    if (now - last_heartbeat >= MQTT_KEEPALIVE_INTERVAL * 1000) {
        // 如果存在待发送的响应，且未超过强制发送阈值（1.5倍间隔），则推迟
        if (pending_property_reply && (now - last_heartbeat < (MQTT_KEEPALIVE_INTERVAL * 1500))) {
            return;
        }
        uint16_t len = build_pingreq(tx_buf);
        if (ESP8266_SendData(tx_buf, len) == 0) {
            last_heartbeat = now;
            ping_missed = 0;
        } else {
            ping_missed++;
        }
    }
    
    // 6. 连续3次心跳无响应，强制重连
    if (ping_missed >= 3) {
        ping_missed = 0;
        ESP8266_ExitTransparent();
        Delay_ms(500);
        ESP8266_ConnectMQTTServer();
        Delay_ms(1000);
        uint16_t len = build_connect(tx_buf);
        ESP8266_SendData(tx_buf, len);
        last_heartbeat = Delay_GetMs();
    }
}

void MQTT_SetPropertyCallback(void (*callback)(char *payload))
{
    property_cb = callback;
}

uint8_t MQTT_IsConnected(void)
{
    return connack_received;
}

