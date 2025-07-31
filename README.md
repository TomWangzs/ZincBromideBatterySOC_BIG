使用实例
#include <stdio.h>
#include "board.h"                     // ← 你的硬件抽象层
#include "hybrid_cc_lstm_ekf_soc.h"    // 头文件

/* 假设系统 100 ms 周期采样一次 */
static void sampling_loop(void)
{
    static float t_prev = 0.0f;

    /* 读取时间戳（秒）——根据自己的时基实现 */
    float t_now = board_get_time_s();
    float dt    = t_now - t_prev;
    t_prev      = t_now;

    /* 读取传感器 */
    float I = board_read_current_A();   // 单位 A
    float V = board_read_voltage_V();   // 单位 V

    /* 更新估算器 */
    float soc = soc_estimator_update(I, V, dt);

    printf("SOC=%.1f %%\r\n", soc * 100.0f);
}

int main(void)
{
    board_init_hardware();

    /* 1) 初始化估算器 */
    soc_estimator_init();

    /* 2) 如果已知当前电池约在 75 %，可选： */
    soc_estimator_set_initial_soc(0.75f);

    /* 3) 周期任务 */
    while (1)
    {
        sampling_loop();
        board_delay_ms(100);            // 100 ms 周期
    }
}
文件结构
├─ hybrid_cc_lstm_ekf_soc_v3_ac5.cpp   // SOC 估算实现
├─ hybrid_cc_lstm_ekf_soc.h           // C 接口头文件（本回复）
├─ weights.h                          // 标准化均值/方差 + 双层 LSTM + FC 权重
├─ weights.cpp
