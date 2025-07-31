
> **为什么多了 `weights.cpp`？**  
> 当权重非常大（> 64 KB）时，直接放在头文件会拖慢增量编译。可采用：  
> 1. `weights.h` 仅保留 `extern const float …` 声明  
> 2. `weights.cpp` 放实际 `const float … = { … };` 定义  
> 3. `weights.cpp` 与 `hybrid_cc_lstm_ekf_soc_v3_ac5.cpp` 一并编译，链接器自动合并

---

## 2. 关键特性

| 功能 | 说明 |
|------|------|
| **充电段：库伦积分 (CC)** | ΔSOC = I·Δt / (η<sub>C</sub> · C<sub>NOM</sub>) |
| **放电段：LSTM + EKF** | LSTM 提供电压-SOC 非线性映射，EKF 自适应融合 & 离群抑制 |
| **纯静态权重** | `weights.(h|cpp)` 在**编译期**链入，定位到 `.rodata` |
| **零动态分配** | 上电即用，RAM 开销 < 4 KB；无 `malloc()` / 文件系统 |
| **多路扩展** | 复制 `SOC_Estimator` 实例或自行封装结构体即可并行估算 |

---

## 3. 编译配置

| 选项                | 作用                    |
|---------------------|-------------------------|
| `--cpp11`           | 启用 C++11              |
| `--no_rtti`         | 去 RTTI，减代码体积     |
| `--no_exceptions`   | 去异常，兼容 Microlib   |
| `-O2` / `-O3`       | 推荐开启优化            |

**Keil uVision 5 设置**  
1. Target → C/C++ (AC5)：勾选 `--cpp11`  
2. Runtime：Microlib（如需极致尺寸）  
3. 把 `soc_estimator/` 所有源文件加入同一 Target / Group  

---

## 4. 快速体验

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






