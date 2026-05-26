#include "stm32f10x.h"
#include "Delay.h"
#include "core_cm3.h" 

static volatile uint32_t sysTickCount = 0;

// DWT 相关寄存器地址定义
#define DWT_CTRL_ADDR           (*(volatile uint32_t *)0xE0001000) // DWT控制寄存器地址
#define DWT_CYCCNT_ADDR         (*(volatile uint32_t *)0xE0001004) // DWT周期计数寄存器地址
#define DEMCR_ADDR              (*(volatile uint32_t *)0xE000EDFC) // 调试异常和监视控制寄存器地址

// 寄存器控制位定义
#define DEMCR_TRCENA            (1 << 24)  // 全局DWT使能位
#define DWT_CTRL_CYCCNTENA      (1 << 0)   // CYCCNT计数器使能位

/**
  * @brief 初始化延时函数（配置 SysTick 为每毫秒中断一次）
  */
void Delay_Init(void)
{
    // 系统时钟频率 SystemCoreClock，设置为 1ms 中断
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 初始化失败，死循环
        while(1);
    }
}

void SysTick_Handler(void)
{
    sysTickCount++;
}

/**
  * @brief 获取系统当前毫秒数
  * @retval 系统运行毫秒数
  */
uint32_t Delay_GetMs(void)
{
    return sysTickCount;
}

/**
 * @brief 初始化DWT硬件计数器
 */
void DWT_Init(void)
{
    // 使能DWT模块 (TRCENA = 1)
    DEMCR_ADDR |= DEMCR_TRCENA;
    // 清零周期计数器
    DWT_CYCCNT_ADDR = 0;
    // 使能周期计数器 (CYCCNTENA = 1)
    DWT_CTRL_ADDR |= DWT_CTRL_CYCCNTENA;
}

/**
 * @brief 微秒级延时 (使用DWT)
 * @param xus 延时时长
 */
void Delay_us(uint32_t xus)
{
    uint32_t start_tick = DWT_CYCCNT_ADDR;
    uint32_t wait_ticks = xus * (SystemCoreClock / 1000000);
    while ((DWT_CYCCNT_ADDR - start_tick) < wait_ticks);
}

void Delay_ms(uint32_t xms)
{
    while (xms--) {
        Delay_us(1000);
    }
}

void Delay_s(uint32_t xs)
{
    while (xs--) {
        Delay_ms(1000);
    }
}

