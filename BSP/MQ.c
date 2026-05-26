#include "MQ.h"
#include "stm32f10x.h"

/* 宏定义 --------------------------------------------------------------------*/
#define WARMUP_TIME_MS   120000U   /* 预热总时长：120秒 = 120000毫秒 */
#define ADC_SAMPLE_TIMES 10        /* 单次浓度计算时的ADC采样次数（用于平均） */

/* 私有函数声明 --------------------------------------------------------------*/
static uint16_t MQ2_Read_ADC(void);   // 读取MQ2的单次ADC原始值
static uint16_t MQ5_Read_ADC(void);   // 读取MQ5的单次ADC原始值

/* 私有全局变量 --------------------------------------------------------------*/
static uint32_t MQ_warmup_start = 0;  /* 预热开始时刻的系统毫秒计数值 */
uint8_t MQ_warmup_done = 0;           /* 预热完成标志，供外部查询 */

/*==============================================================================
 * 函数名称：MQ_StartWarmup
 * 功能描述：记录当前系统时间作为预热起点，并将预热完成标志清零。
 * 调用时机：在 main() 初始化完 MQ_Init() 后立即调用一次。
 *============================================================================*/
void MQ_StartWarmup(void)
{
    MQ_warmup_start = Delay_GetMs();  // 获取当前系统运行毫秒数
    MQ_warmup_done = 0;               // 重置预热完成标志
}

/*==============================================================================
 * 函数名称：MQ_Init
 * 功能描述：初始化 PA3、PA4 为模拟输入，配置 ADC1 为单次转换模式并校准。
 * 硬件对应：MQ2 → PA3 (ADC通道3)，MQ5 → PA4 (ADC通道4)
 *============================================================================*/
void MQ_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    /* 1. 开启 GPIOA 和 ADC1 的时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    /* 2. 配置 PA3、PA4 为模拟输入模式（AIN） */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;   // 模拟输入，不经过内部上下拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3. 配置 ADC1 基本参数 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;      // 独立模式（非多ADC同步）
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;                   // 关闭扫描（单通道转换）
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                   // 单次转换（非连续）
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None; // 软件触发
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;       // 12位数据右对齐
    ADC_InitStructure.ADC_NbrOfChannel       = 1;                         // 规则组通道数量
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 4. 使能 ADC1 */
    ADC_Cmd(ADC1, ENABLE);

    /* 5. 执行 ADC 校准（必须） */
    ADC_ResetCalibration(ADC1);                     // 复位校准寄存器
    while (ADC_GetResetCalibrationStatus(ADC1));    // 等待复位完成
    ADC_StartCalibration(ADC1);                     // 开始校准
    while (ADC_GetCalibrationStatus(ADC1));         // 等待校准完成
}

/*==============================================================================
 * 函数名称：MQ2_Read_ADC
 * 功能描述：读取 MQ2（PA3）的单次 ADC 转换结果。
 * 返回值  ：0 ~ 4095 的原始 ADC 数值。
 * 注意事项：每次调用会重新配置通道并启动一次软件转换。
 *============================================================================*/
static uint16_t MQ2_Read_ADC(void)
{
    /* 配置规则组通道：ADC1通道3，采样时间 239.5 周期（足够稳定） */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_239Cycles5);
    
    /* 软件启动一次转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    
    /* 等待转换结束（EOC 标志置位） */
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    
    /* 返回转换结果并自动清除 EOC 标志 */
    return ADC_GetConversionValue(ADC1);
}

/*==============================================================================
 * 函数名称：MQ5_Read_ADC
 * 功能描述：读取 MQ5（PA4）的单次 ADC 转换结果。
 * 返回值  ：0 ~ 4095 的原始 ADC 数值。
 *============================================================================*/
static uint16_t MQ5_Read_ADC(void)
{
    /* 配置规则组通道：ADC1通道4，采样时间 239.5 周期 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_239Cycles5);
    
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/*==============================================================================
 * 函数名称：MQ2_GetValue
 * 功能描述：多次采样求平均，返回 MQ2 的原始 ADC 平均值。
 * 返回值  ：0 ~ 4095
 *============================================================================*/
uint16_t MQ2_GetValue(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLE_TIMES; i++) {
        sum += MQ2_Read_ADC();
    }
    return (uint16_t)(sum / ADC_SAMPLE_TIMES);
}

/*==============================================================================
 * 函数名称：MQ5_GetValue
 * 功能描述：多次采样求平均，返回 MQ5 的原始 ADC 平均值。
 * 返回值  ：0 ~ 4095
 *============================================================================*/
uint16_t MQ5_GetValue(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLE_TIMES; i++) {
        sum += MQ5_Read_ADC();
    }
    return (uint16_t)(sum / ADC_SAMPLE_TIMES);
}

/*==============================================================================
 * 函数名称：MQ2_GetSmoke
 * 功能描述：获取 MQ2 检测到的烟雾浓度百分比（0~100%）。
 * 计算方式：对 ADC 原始值求平均，然后按线性比例映射到 0~100%。
 * 注意：该映射仅为粗略近似，真实浓度与 ADC 值呈非线性关系。
 *============================================================================*/
uint16_t MQ2_GetSmoke(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLE_TIMES; i++) {
        sum += MQ2_Read_ADC();
    }
    uint16_t adc_avg = (uint16_t)(sum / ADC_SAMPLE_TIMES);
    
    /* 线性映射：0 → 0%，4095 → 100% */
    return (uint16_t)((adc_avg * 100UL) / 4095UL);
}

/*==============================================================================
 * 函数名称：MQ5_GetGas
 * 功能描述：获取 MQ5 检测到的天然气浓度百分比（0~100%）。
 * 计算方式：与 MQ2_GetSmoke 相同。
 *============================================================================*/
uint16_t MQ5_GetGas(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLE_TIMES; i++) {
        sum += MQ5_Read_ADC();
    }
    uint16_t adc_avg = (uint16_t)(sum / ADC_SAMPLE_TIMES);
    
    return (uint16_t)((adc_avg * 100UL) / 4095UL);
}

/*==============================================================================
 * 函数名称：MQ_Get
 * 功能描述：核心数据处理函数。
 *          1. 读取烟雾和天然气浓度值并更新全局变量 smoke / gas。
 *          2. 检查预热计时，若未完成则强制清零所有报警标志，不执行报警判断。
 *          3. 预热完成后，根据浓度阈值更新 MQ2_flag、MQ5_flag 和 buzzer_flag。
 * 调用关系：由 MQ_CheckAlarm() 调用，也可在其他需要读取浓度的地方直接调用。
 *============================================================================*/
void MQ_Get(void)
{
    /* --- 第一步：读取传感器浓度（不管是否预热，数值都要更新，供 OLED 显示） --- */
    smoke = MQ2_GetSmoke();
    gas   = MQ5_GetGas();

    /* --- 第二步：处理预热逻辑 --- */
    if (!MQ_warmup_done)   // 预热尚未完成
    {
        /* 检查预热时间是否已经达到预设值 */
        if ((Delay_GetMs() - MQ_warmup_start) >= WARMUP_TIME_MS)
        {
            MQ_warmup_done = 1;   // 预热完成，后续循环将进入正常报警判断
        }
        else
        {
            /* 预热期间：传感器输出不稳定，强制关闭所有报警标志 */
            MQ2_flag = 0;
            MQ5_flag = 0;
            buzzer_flag = 0;
            return;   // 直接返回，不执行下面的报警阈值判断
        }
    }

    /* --- 第三步：预热完成后的正常报警逻辑 --- */
    /* 烟雾浓度超过 95% 时报警 */
    if (smoke > 95)
    {
        MQ2_flag = 1;
        buzzer_flag = 1;
    }
    /* 天然气浓度超过 30% 时报警 */
    else if (gas > 30)
    {
        MQ5_flag = 1;
        buzzer_flag = 1;
    }
    /* 均未超阈值，清除所有报警标志 */
    else
    {
        MQ2_flag = 0;
        MQ5_flag = 0;
        buzzer_flag = 0;
    }
}

/*==============================================================================
 * 函数名称：MQ_CheckAlarm
 * 功能描述：统一报警入口。
 *          1. 调用 MQ_Get() 更新浓度和报警标志。
 *          2. 根据 buzzer_flag 控制蜂鸣器的开关。
 * 调用位置：主循环中周期性调用。
 *============================================================================*/
void MQ_CheckAlarm(void)
{
    MQ_Get();   // 执行浓度读取和报警判断（内含预热处理）

    /* 根据报警标志控制蜂鸣器 */
    if (buzzer_flag == 1)
    {
        Buzzer_On();
    }
    else
    {
        Buzzer_Off();
    }
}
