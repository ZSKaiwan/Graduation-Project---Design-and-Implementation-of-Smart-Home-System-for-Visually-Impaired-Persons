#include "stm32f10x.h"
#include "Var.h"

/*功能状态״̬*/

uint8_t Window_Mode = 0;     //程序内部用：0关1开
uint8_t Servo = 0;			//上传做标识用： 0关1开
uint8_t WindowN_Value = 0;	//内侧0度关闭

uint8_t WindowW_Value = 60;	//外侧60度关闭

uint8_t MQ2_flag = 0;        //烟雾浓度报警标志位 0关1开
uint8_t MQ5_flag = 0;        //燃气浓度报警标志位 0关1开
uint8_t buzzer_flag = 0;         //蜂鸣器标志位	0关1开


//将传感器读取的数据转化为小数并上传到服务器
float temp;      //温度
float humi;      //湿度

uint16_t smoke = 0 ;    // 烟雾浓度
uint16_t gas  =0  ;    // 天然气浓度

//已连上服务器，允许读取传感器数据标志位
unsigned int  Start_flag=0,Start_flag1 = 1;		//网络连接标志位 0失败1成功
unsigned int  Contrue_flag=0;
unsigned int  Conflas_flag=0;

// 全局变量定义（实际创建变量）
unsigned int Humi_int = 0;       // 湿度整数部分
unsigned int Humi_deci = 0;      // 湿度小数部分
unsigned int Temp_int = 0;       // 温度整数部分
unsigned int Temp_deci = 0;      // 温度小数部分

// 传感器读取的数据结果放大10倍
uint16_t temperature = 0;        // 温度值
uint16_t humidity = 0;           // 湿度值



