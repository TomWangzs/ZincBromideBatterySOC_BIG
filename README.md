Hybrid CC-LSTM-EKF SOC 估算库 — README（示例工程版）
适用场景

大容量锌溴液流电池（或其他电化学体系）SOC 在线估算

MCU / DSP / SoC 单机多路 BMS

ARM Compiler 5（或兼容 AC5 指令集）

无文件系统、无动态内存、实时性要求高的嵌入式平台

1. 目录结构
bash
复制
编辑
soc_estimator/                       ← 建议单独放一个文件夹
├─ hybrid_cc_lstm_ekf_soc_v3_ac5.cpp   # 核心实现（C++11，无 RTTI & 异常）
├─ hybrid_cc_lstm_ekf_soc.h            # C 语言友好的外部接口
├─ weights.h                           # 常量形式的权重（由 Python 导出）
└─ weights.cpp                         # 可选：若权重较大，拆到 .cpp⇆.h 双文件
user_code/                           ← 你的应用层代码
└─ main.c                              # 采样 / 打印 / 业务逻辑示例
为什么多了 weights.cpp？

当权重非常大（>64 KB）时，直接写在头文件会让编译依赖复杂、增量编译缓慢。

拆分方案：

weights.h 只保留 extern const float ... 声明；

weights.cpp 放实际的 const float ... = { ... }; 定义；

weights.cpp 与 hybrid_cc_lstm_ekf_soc_v3_ac5.cpp 一并编译，链接器自动合并。

2. 关键特性
功能	说明
充电段：库伦积分 (CC)	ΔSOC = I·Δt / (η<sub>C</sub> C<sub>NOM</sub>)
放电段：LSTM + EKF	LSTM 提供电压-SOC 非线性映射，EKF 自适应融合 & 离群抑制
纯静态权重	`weights.(h
零动态分配	上电即用，RAM 开销 < 4 KB；无 malloc()、无文件系统
多路扩展	复制 SOC_Estimator 实例或自行封装结构体即可并行估算

3. 编译配置
选项	作用
--cpp11	启用 C++11（auto/constexpr 等）
--no_rtti	去掉 RTTI，减代码 & Flash
--no_exceptions	去掉异常机制，兼容 Microlib
-O2/-O3	推荐开启优化

Keil uVision 5

在 Target Options → C/C++ (AC5) 里勾选 --cpp11。

在 Runtime 选择 Microlib（如需极致尺寸）。

把 soc_estimator/ 源文件全部加入同一个 Group。

4. 快速体验
c
复制
编辑
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
5. API 速查
函数	何时调用	作用
void soc_estimator_init(void)	上电 / 复位	载入权重、清零状态
float soc_estimator_set_initial_soc(float init_soc)	可选，init() 之后、第一次 update() 之前	设定 SOC 起点，返回裁剪后值
float soc_estimator_update(float I, float V, float dt)	每个采样周期	返回最新 SOC（0-1）

6. FAQ
问题	排查 / 解答
SOC 一直不变	① dt 是否为 0？② ADC 电流量程是否正确？
编译报 “undefined symbol”	忘记把 .cpp 文件加入工程；或 .h / .cpp 名称不一致
Flash 占用过大	尝试减小 LSTM 隐层或量化权重；或启用 --data_reorder 压缩
需要多通道	给每路电池各建一个 SOC_Estimator 实例，或复制 C 接口并改名

7. 更新权重流程
bash
复制
编辑
# 1. 训练好新模型后
python export_weights.py \
       --ckpt lstm_soc_discharge_big.pth \
       --outdir soc_estimator/

# 2. 得到新的 weights.h / weights.cpp
# 3. 重新编译固件即可；业务代码无需改动
© 2025 Zinc-Bromine Battery Lab
如有问题，请联系 support@example.com 或提交 Issue。





