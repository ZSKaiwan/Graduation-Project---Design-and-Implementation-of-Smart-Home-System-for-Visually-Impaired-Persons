#include "MQTT.h"
#include "ESP8266.h"
#include "Delay.h"
#include <string.h>
#include <stdio.h>
#include "Var.h"
#include "Buzzer.h"
/*-------------------------- 内部缓冲区 --------------------------*/
 uint8_t tx_buf[512];                     // 发送缓冲区
 uint8_t rx_buf[512];                     // 接收缓冲区（存储 PUBLISH 载荷）
 uint32_t last_heartbeat = 0;             // 上次心跳时间（毫秒）
 void (*property_cb)(char *payload) = NULL; // 属性回调
 
/* ========== 修改5：新增外部标志声明 ========== */
extern volatile uint8_t  pending_puback;
extern volatile uint16_t pending_packet_id;
/* =========================================== */ 
extern uint8_t subscribed;


/*-------------------------- MQTT 接收状态机 --------------------------*/
typedef enum {
    MQTT_RX_IDLE,           // 空闲，等待固定头
    MQTT_RX_REM_LEN,        // 正在读取剩余长度（多字节）
    MQTT_RX_PAYLOAD         // 正在读取载荷
} MqttRxState;

static MqttRxState rx_state = MQTT_RX_IDLE;
static uint8_t fixed_header;            // 存储固定头的第一个字节
static uint32_t remaining_len = 0;      // 剩余长度（解码后）
static uint8_t rem_len_bytes = 0;       // 已读取的剩余长度字节数
static uint16_t payload_index = 0;      // 当前载荷读取位置

/*-------------------------- 连接和心跳标志 --------------------------*/
static uint8_t connack_received = 0;    // 是否收到 CONNACK
static uint8_t ping_missed = 0;         // 连续心跳无响应计数

/**
 * @brief 将剩余长度编码为 MQTT 可变字节格式
 * @param buf 输出缓冲区
 * @param len 原始剩余长度值
 * @return 编码后的字节数
 */
uint8_t encode_remaining_length(uint8_t *buf, uint32_t len)
{
    uint8_t pos = 0;
    do {
        uint8_t digit = len % 128;
        len /= 128;
        if (len > 0) {
            digit |= 0x80;      // 最高位置1表示后续还有字节
        }
        buf[pos++] = digit;
    } while (len > 0 && pos < 4);
    return pos;
}

/*-------------------------- 解码剩余长度（从数据流中读取）--------------------------*/
uint32_t decode_remaining_length(uint8_t **ptr, uint16_t *available)
{
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encodedByte;
    uint8_t bytes_read = 0;
    
    do {
        if (*available == 0) return 0;
        encodedByte = **ptr;
        (*ptr)++;
        (*available)--;
        value += (encodedByte & 0x7F) * multiplier;
        multiplier *= 128;
        bytes_read++;
        if (multiplier > 128*128*128) return 0;
    } while ((encodedByte & 0x80) != 0 && bytes_read < 4);
    
    return value;
}

/*-------------------------- 构建 CONNECT 报文 --------------------------*/
uint16_t build_connect(uint8_t *pkt)
{
    uint16_t pos = 0;
    uint32_t rem_len;
    uint8_t len_buf[4];
    uint8_t len_bytes;
    
    pkt[pos++] = 0x10;  // CONNECT
    
    rem_len = 10 + 2 + strlen(DEVICE_NAME) + 2 + strlen(PRODUCT_ID) + 2 + strlen(PASSWORD);
    len_bytes = encode_remaining_length(len_buf, rem_len);
    memcpy(&pkt[pos], len_buf, len_bytes);
    pos += len_bytes;
    
    pkt[pos++] = 0x00; pkt[pos++] = 0x04;
    memcpy(&pkt[pos], "MQTT", 4); pos += 4;
    pkt[pos++] = 0x04;               // 协议级别 4 (MQTT 3.1.1)
    pkt[pos++] = 0xC2;               // 连接标志：Clean Session, 用户名/密码
    pkt[pos++] = 0x00;
	pkt[pos++] = 0x64; // Keep Alive 100秒
    
    pkt[pos++] = (strlen(DEVICE_NAME) >> 8) & 0xFF;
    pkt[pos++] = strlen(DEVICE_NAME) & 0xFF;
    memcpy(&pkt[pos], DEVICE_NAME, strlen(DEVICE_NAME)); pos += strlen(DEVICE_NAME);
    
    pkt[pos++] = (strlen(PRODUCT_ID) >> 8) & 0xFF;
    pkt[pos++] = strlen(PRODUCT_ID) & 0xFF;
    memcpy(&pkt[pos], PRODUCT_ID, strlen(PRODUCT_ID)); pos += strlen(PRODUCT_ID);
    
    pkt[pos++] = (strlen(PASSWORD) >> 8) & 0xFF;
    pkt[pos++] = strlen(PASSWORD) & 0xFF;
    memcpy(&pkt[pos], PASSWORD, strlen(PASSWORD)); pos += strlen(PASSWORD);
    
    return pos;
}

/*-------------------------- 构建 PUBLISH 报文 (QoS=0) --------------------------*/
uint16_t build_publish(uint8_t *pkt, const char *topic, const char *payload)
{
    uint16_t pos = 0;
    uint32_t rem_len;
    uint8_t len_buf[4];
    uint8_t len_bytes;
    
    pkt[pos++] = 0x30;  // PUBLISH, QoS=0
    //计算剩余长度
    rem_len = 2 + strlen(topic) + strlen(payload);
	//编码剩余长度
    len_bytes = encode_remaining_length(len_buf, rem_len);
    memcpy(&pkt[pos], len_buf, len_bytes);
    pos += len_bytes;
    //主题
    pkt[pos++] = (strlen(topic) >> 8) & 0xFF;
    pkt[pos++] = strlen(topic) & 0xFF;
    memcpy(&pkt[pos], topic, strlen(topic)); pos += strlen(topic);
    //有效载荷
    memcpy(&pkt[pos], payload, strlen(payload)); pos += strlen(payload);
    
    return pos;
}

/**
 * @brief 构建 MQTT SUBSCRIBE 报文（订阅 OneNET 属性设置主题）
 * @param pkt 输出缓冲区指针（至少需要约 128 字节空间）
 * @return 报文总长度（字节数）
 * @note  主题通过常量指针拼接，不占用栈空间，安全可靠
 */
/*-------------------------- 构建 SUBSCRIBE 报文（栈安全版） --------------------------*/
uint16_t build_subscribe(uint8_t *pkt)
{
    uint16_t pos = 0;
    uint32_t rem_len;
    uint8_t len_buf[4];
    uint8_t len_bytes;
    
    // 主题通过宏拼接为常量字符串，存储在 Flash 常量区，不占用栈空间
    const char *topic = "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set";
    uint16_t topic_len = strlen(topic);
    
    pkt[pos++] = 0x82;  // SUBSCRIBE, QoS=1
    
    // 剩余长度 = PacketID(2) + 主题长度字段(2) + 主题内容长度 + QoS(1)
    rem_len = 2 + 2 + topic_len + 1;
    
    // 编码剩余长度
    len_bytes = encode_remaining_length(len_buf, rem_len);
    memcpy(&pkt[pos], len_buf, len_bytes);
    pos += len_bytes;
    
    // Packet Identifier
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x01;
    
    // 主题长度（大端序）
    pkt[pos++] = (topic_len >> 8) & 0xFF;
    pkt[pos++] = topic_len & 0xFF;
    
    // 主题内容
    memcpy(&pkt[pos], topic, topic_len);
    pos += topic_len;
    
    // QoS = 0
    pkt[pos++] = 0x00;
    
    return pos;
}

/*-------------------------- 构建 PINGREQ 报文 --------------------------*/
uint16_t build_pingreq(uint8_t *pkt)
{
    pkt[0] = 0xC0;
    pkt[1] = 0x00;
    return 2;
}

/*-------------------------- 解析 PUBLISH 报文载荷 --------------------------*/
void parse_publish_payload(uint8_t *data, uint32_t len)
{
    if (len < 2) return;
    uint8_t *ptr = data;
    uint8_t header = *ptr++;
    uint32_t remaining = 0;
    uint16_t available = len - 1;
    
    remaining = decode_remaining_length(&ptr, &available);
    if (remaining == 0) return;
    
    if (available < 2) return;
    uint16_t topic_len = (ptr[0] << 8) | ptr[1];
    ptr += 2;
    available -= 2;
    if (available < topic_len) return;
    ptr += topic_len;
    available -= topic_len;
    
    uint32_t payload_len = remaining - (2 + topic_len);
    if (payload_len > 0 && property_cb) {
		
        char payload_str[256];
        if (payload_len < sizeof(payload_str)) {
            memcpy(payload_str, ptr, payload_len);
            payload_str[payload_len] = '\0';
            property_cb(payload_str);
        }
    }
}

/*-------------------------- 接收一个字节（状态机处理）--------------------------*/
void MQTT_ReceiveByte(uint8_t byte)
{
    switch (rx_state) {
        case MQTT_RX_IDLE:
            fixed_header = byte;
            rx_state = MQTT_RX_REM_LEN;
            remaining_len = 0;
            rem_len_bytes = 0;
            payload_index = 0;
            // 将固定头存入 rx_buf（用于 PUBLISH 报文完整解析）
            if (payload_index < sizeof(rx_buf)) {
                rx_buf[payload_index++] = byte;
            }
            break;
            
        case MQTT_RX_REM_LEN:
            remaining_len |= (byte & 0x7F) << (7 * rem_len_bytes);
            rem_len_bytes++;
            // 剩余长度字节也存入缓冲区
            if (payload_index < sizeof(rx_buf)) {
                rx_buf[payload_index++] = byte;
            }
            if ((byte & 0x80) == 0) {
                // 剩余长度读取完毕，立即判断 CONNACK 以便快速置位连接标志
                if ((fixed_header & 0xF0) == 0x20) {
                    connack_received = 1;
                }
                if (remaining_len > 0) {
                    rx_state = MQTT_RX_PAYLOAD;
                } else {
                    // 无载荷报文处理（如 PINGRESP）
                    if ((fixed_header & 0xF0) == 0xD0) {
                        ping_missed = 0;
                    }
                    rx_state = MQTT_RX_IDLE;
                }
            } else if (rem_len_bytes >= 4) {
                rx_state = MQTT_RX_IDLE;
            }
            break;
            
        case MQTT_RX_PAYLOAD:
            if (payload_index < sizeof(rx_buf)) {
                rx_buf[payload_index++] = byte;
                // 计算已收到的载荷部分长度（跳过固定头和剩余长度字段）
                uint16_t header_and_rem_len_bytes = 1 + rem_len_bytes;
                uint16_t received_payload = payload_index - header_and_rem_len_bytes;
                if (received_payload >= remaining_len) {
                    // 完整报文接收完毕
                    if ((fixed_header & 0xF0) == 0x30) { // PUBLISH
                        // 定位到可变头（跳过固定头和剩余长度）
                        uint8_t *ptr = rx_buf + header_and_rem_len_bytes;
                        // 读取主题长度
                        uint16_t topic_len = (ptr[0] << 8) | ptr[1];
                        ptr += 2;						
                        // 跳过主题内容
                        ptr += topic_len;
                        // 获取 QoS 等级
                        uint8_t qos = (fixed_header & 0x06) >> 1;
                        uint16_t packet_id = 0;
                        if (qos > 0) {
                            // 提取 Packet Identifier（大端序）
                            packet_id = (ptr[0] << 8) | ptr[1];
                            ptr += 2;
                        }
						
						/* === 修改7：记录 PUBACK 待发送标志，不在此处发送 === */
                        if (qos > 0) {
                            pending_puback = 1;
                            pending_packet_id = packet_id;
                        }
                        /* ================================================== */

                        // 计算载荷长度并调用回调
                        uint32_t payload_len = remaining_len - (2 + topic_len + (qos > 0 ? 2 : 0));
                        if (payload_len > 0 && property_cb) {
                            char payload_str[256];
                            if (payload_len < sizeof(payload_str)) {
                                memcpy(payload_str, ptr, payload_len);
                                payload_str[payload_len] = '\0';
                                property_cb(payload_str);  // 回调不再包含发送操作
                            }
                        }
                    } else if ((fixed_header & 0xF0) == 0x20) {
                        connack_received = 1;
                    }
                    rx_state = MQTT_RX_IDLE;
                }
            } else {
                rx_state = MQTT_RX_IDLE;
            }
            break;
    }
}

/*-------------------------- 等待 CONNACK 响应（使用标志）--------------------------*/
uint8_t wait_connack(uint32_t timeout_ms)
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
void MQTT_Init(void)
{
    uint16_t len;
	
    // 发送 CONNECT
    len = build_connect(tx_buf);
    //ESP8266_SendRawData(tx_buf, len);
	ESP8266_SendData(tx_buf, len); 

	 // ====== 初始化心跳计时变量 ======
    last_heartbeat = Delay_GetMs();
    ping_missed = 0;
	subscribed = 0;   // 新连接需要重新订阅
}
/**
 * @brief  MQTT 上报所有传感器数据（温湿度、烟雾、天然气、舵机状态）
 * @param  无（数据从全局变量中获取）
 * @retval 无
 */
void MQTT_ReportAllSensors(void)
{

    char topic[128];
    char payload[256];
    uint16_t len;
	
    // 将全局温度、湿度（放大10倍的整数）转换为浮点数（保留一位小数）
    float temperature_f = temperature / 10.0f;
    float humidity_f    = humidity    / 10.0f;
	
	// 将 Servo 数值转换为布尔字符串
    char *servo_str = (Servo != 0) ? "true" : "false";
	
	// ==== 新增：获取蜂鸣器当前状态（通过读取GPIO或维护一个全局变量） ====
    // 假设维护了一个全局变量 buzzer_state，在 Buzzer_On/Off 时同步更新
    extern uint8_t buzzer_state;   // 需在 Buzzer.c 或 main.c 中定义
    char *buzzer_str = buzzer_state ? "true" : "false";
    // =================================================================
	
    // 烟雾浓度、天然气浓度：0~100，直接使用整型
    // 舵机状态：假设 Servo = 1 表示开，0 表示关（根据您的 Var.h 中 extern uint8_t Servo）
    sprintf(topic, TOPIC_PROPERTY_POST, PRODUCT_ID, DEVICE_NAME);
	sprintf(payload,
        "{\"id\":\"123\",\"params\":{"
        "\"temp\":{\"value\":%.1f},"
        "\"humi\":{\"value\":%.1f},"
        "\"smoke\":{\"value\":%d},"
        "\"gas\":{\"value\":%d},"
        "\"Servo\":{\"value\":%s},"
		"\"Buzzer\":{\"value\":%s}"
        "}}",
        temp, humi,
        smoke, gas,
        servo_str,
		buzzer_str);

    len = build_publish(tx_buf, topic, payload);
	ESP8266_SendData(tx_buf, len); 

}
/* ========== MQTT_Process —— 增加自动订阅重试 ========== */
/*-------------------------- 心跳和自动重连--------------------------*/
void MQTT_Process(void)
{
	 uint32_t now = Delay_GetMs();
    
	 if (!ESP8266_IsConnected()) {
        ping_missed = 0;
        subscribed = 0;                    // 重置订阅标志
        ESP8266_ConnectMQTTServer();
        Delay_ms(1000);
        uint16_t len = build_connect(tx_buf);
        ESP8266_SendData(tx_buf, len);
        last_heartbeat = Delay_GetMs();
        return;
    }
    
    /* === 连接成功但未订阅时，自动发送订阅报文 === */
    if (connack_received && !subscribed) {
        uint16_t sub_len = build_subscribe(tx_buf);
        if (ESP8266_SendData(tx_buf, sub_len) == 0) {
            subscribed = 1;
        }
    }
	// 检查心跳间隔
		if (now - last_heartbeat >= MQTT_KEEPALIVE_INTERVAL * 1000) {
			// 如果有待发送的响应，且未超过强制发送阈值（例如1.5倍间隔），则推迟
			extern volatile uint8_t pending_property_reply;
			if (pending_property_reply && (now - last_heartbeat < (MQTT_KEEPALIVE_INTERVAL * 1500))) {
				// 推迟，不更新 last_heartbeat，下次循环再判断
				return;
			}
        // 状态正常，发送心跳
        uint16_t len = build_pingreq(tx_buf);
        if (ESP8266_SendData(tx_buf, len) == 0) {
            last_heartbeat = now;
            ping_missed = 0;
        } else {
            ping_missed++;
        }
    }
    
    if (ping_missed >= 3) {
		ping_missed = 0;
		ESP8266_ExitTransparent();
		Delay_ms(500);
		ESP8266_ConnectMQTTServer();
		Delay_ms(1000);
		uint16_t len = build_connect(tx_buf);
		ESP8266_SendData(tx_buf, len);
		// 不再调用 wait_connack，由主循环自然接收 CONNACK
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
