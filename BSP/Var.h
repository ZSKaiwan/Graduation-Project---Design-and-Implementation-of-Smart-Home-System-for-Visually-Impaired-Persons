#ifndef __VAR_H
#define __VAR_H


/*功能状态标志̬*/

extern uint8_t Window_Mode;     //窗户状态
extern uint8_t Servo; 
extern uint8_t WindowN_Value;   //内侧compareֵ
extern uint8_t WindowW_Value;	//外侧compareֵ

extern uint8_t MQ2_flag;        //烟雾浓度报警标志位
extern uint8_t MQ5_flag;        //燃气浓度报警标志位

extern uint8_t buzzer_flag;        //蜂鸣器标志位

//将传感器读取的数据转化为小数并上传到服务器
extern float temp;      //温度
extern float humi;      //湿度

extern uint16_t smoke ;    // 烟雾浓度
extern uint16_t gas   ;    // 天然气浓度

//已连上服务器，允许读取传感器数据标志位
extern unsigned int  Start_flag;	//网络连接标志位
extern unsigned int Start_flag1;    //传感器可传输标志位
extern unsigned int  Contrue_flag;	// 收到云端下发 "tru" → 开
extern unsigned int  Conflas_flag;  // 收到云端下发 "fal" → 关     

// 全局变量声明（extern 关键字）
extern unsigned int Humi_int;    // 湿度整数部分
extern unsigned int Humi_deci;   // 湿度小数部分
extern unsigned int Temp_int;    // 温度整数部分
extern unsigned int Temp_deci;   // 温度小数部分

// 传感器数据放大10倍
extern uint16_t temperature;     // 温度值
extern uint16_t humidity;        // 湿度值

extern char buffer[64];

#endif

