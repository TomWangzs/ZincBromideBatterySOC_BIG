

## 1. 关键特性

| 功能 | 说明 |
|------|------|
| **充电段：库伦积分 (CC)** | ΔSOC = I·Δt / (η<sub>C</sub> · C<sub>NOM</sub>) |
| **放电段：LSTM + EKF** | LSTM 提供电压-SOC 非线性映射，EKF 自适应融合 & 离群抑制 |
| **纯静态权重** | `weights.h/weights.cpp` 在**编译期**链入，定位到 `.rodata` |
| **零动态分配** | 上电即用，RAM 开销 < 4 KB；无 `malloc()` / 文件系统 |
| **多路扩展** | 复制 `SOC_Estimator` 实例或自行封装结构体即可并行估算 |

---

## 2. 快速体验

```c
#include <stdio.h>
#include "board.h"                      // ← 你的硬件抽象层
#include "hybrid_cc_lstm_ekf_soc.h"     // 估算器接口

/* 假设系统 100 ms 周期采样一次 */
static void sampling_loop(void)
{
    static float t_prev = 0.0f;

    float t_now = board_get_time_s();           // 秒
    float dt    = t_now - t_prev;
    t_prev      = t_now;

    float I = board_read_current_A();           // A
    float V = board_read_voltage_V();           // V

    float soc = soc_estimator_update(I, V, dt); // 0~1

    printf("SOC = %.1f %%\r\n", soc * 100.0f);
}

int main(void)
{
    board_init_hardware();

    /* 1) 初始化 */
    soc_estimator_init();

    /* 2) 若已知当前约 75 %： */
    soc_estimator_set_initial_soc(0.75f);

    /* 3) 主循环 */
    while (1)
    {
        sampling_loop();
        board_delay_ms(100);                    // 100 ms 周期
    }
}






