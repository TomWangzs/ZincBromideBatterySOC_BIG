
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "weights.h"         // <<< 4 维 mean/scale & LSTM/FC 权重


static const float    C_NOM        = 640.0f;      // 电池额定容量 / Ah
static const float    ETA_C        = 1.0f;        // 充电库伦效率
static const float    ETA_D        = 0.8f;        // 放电库伦效率
static const float    Q_PROC       = 1e-10f;      // 过程噪声方差（EKF）
static const float    R_MEAS_BASE  = 0.15f * 0.15f * 0.15f * 0.15f;
static const float    ERR_TH       = 0.08f;       // 离群剔除阈值
static const float    GATE         = 0.001f;      // 动态 R 基准
static const float    CUM_AH_OFFSET= 0.0f;        // 首轮 cum_Ah 偏移
static const uint32_t SEQ_LEN      = 10u;         // LSTM 时间步
static const uint32_t INPUT_SIZE   = 4u;          // 输入维度：I, V, dV/dt, cum_Ah
static const uint32_t HIDDEN_SIZE  = 64u;         // LSTM 隐层宽度
static const uint32_t NUM_LAYERS   = 2u;          // LSTM 层数


static inline float sigmoid_f(float x) { return 1.f / (1.f + std::exp(-x)); }

static inline float tanh_f   (float x) { return std::tanh(x); }

static inline float clip01   (float x)
{
    return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
}

struct StandardScaler
{
    float mean [INPUT_SIZE];
    float scale[INPUT_SIZE];


    void transform(const float in_[INPUT_SIZE],
                   float       out_[INPUT_SIZE]) const
    {
        for (uint32_t i = 0; i < INPUT_SIZE; ++i)
        {
            // (x - μ) / σ
            out_[i] = (in_[i] - mean[i]) / scale[i];
        }
    }
};


struct LSTMCell
{
    uint32_t     in_size;    // 当前层输入维度
    const float* W_ih;       // [4H, in_size]  输入权重
    const float* W_hh;       // [4H, H]        隐状态权重
    const float* b_ih;       // [4H]           输入偏置
    const float* b_hh;       // [4H]           隐状态偏置


    void step(const float* x,
              float       h[HIDDEN_SIZE],
              float       c[HIDDEN_SIZE]) const
    {
        float gates[4 * HIDDEN_SIZE];                 // i, f, g, o

        // ------- 1) x * W_ih^T + b_ih -------
        for (uint32_t g = 0; g < 4 * HIDDEN_SIZE; ++g)
        {
            float sum = b_ih[g];
            const float* w = W_ih + g * in_size;
            for (uint32_t j = 0; j < in_size; ++j)
                sum += w[j] * x[j];
            gates[g] = sum;
        }

        // ------- 2) h_prev * W_hh^T + b_hh -------
        for (uint32_t g = 0; g < 4 * HIDDEN_SIZE; ++g)
        {
            float sum = b_hh[g];
            const float* w = W_hh + g * HIDDEN_SIZE;
            for (uint32_t j = 0; j < HIDDEN_SIZE; ++j)
                sum += w[j] * h[j];
            gates[g] += sum;
        }

        // ------- 3) 激活 & 更新状态 -------
        const float* i_gate = gates + 0 * HIDDEN_SIZE;
        const float* f_gate = gates + 1 * HIDDEN_SIZE;
        const float* g_gate = gates + 2 * HIDDEN_SIZE;
        const float* o_gate = gates + 3 * HIDDEN_SIZE;

        for (uint32_t k = 0; k < HIDDEN_SIZE; ++k)
        {
            float i_t = sigmoid_f(i_gate[k]);
            float f_t = sigmoid_f(f_gate[k]);
            float g_t = tanh_f   (g_gate[k]);
            float o_t = sigmoid_f(o_gate[k]);

            c[k] = f_t * c[k] + i_t * g_t;
            h[k] = o_t * tanh_f(c[k]);
        }
    }
};


struct LSTMNetwork
{
    LSTMCell      layers[NUM_LAYERS];
    const float*  fc_weight;        // [H]
    float         fc_bias;


    float forward(const float seq[][INPUT_SIZE]) const
    {
        float h[NUM_LAYERS][HIDDEN_SIZE] = {{0}};
        float c[NUM_LAYERS][HIDDEN_SIZE] = {{0}};

        // ---------- 时间展开 ----------
        for (uint32_t t = 0; t < SEQ_LEN; ++t)
        {
            layers[0].step(seq[t], h[0], c[0]);   // 第一层
            layers[1].step(h[0],   h[1], c[1]);   // 第二层
        }

        // ---------- FC head ----------
        float sum = fc_bias;
        for (uint32_t j = 0; j < HIDDEN_SIZE; ++j)
            sum += fc_weight[j] * h[1][j];

        return sigmoid_f(sum);                   // ★ 输出范围 0~1
    }
};

// ============================================================================
//  ↓↓↓                         EKF 工具函数                             ↓↓↓
// ============================================================================
static inline void ekf_update(float x_pred, float P_pred,
                              float z, float R_meas,
                              float& x_upd, float& P_upd)
{
    // 卡尔曼增益
    float S = P_pred + R_meas;
    float K = P_pred / S;

    // 状态 & 协方差更新
    x_upd   = x_pred + K * (z - x_pred);
    P_upd   = (1.f - K) * P_pred + Q_PROC;
}


struct SlidingWindow
{
    float    buf[SEQ_LEN][INPUT_SIZE];   // 环形缓冲区
    uint32_t idx;                        // 写指针
    bool     filled;                     // 是否已填满一个窗口

    SlidingWindow() : idx(0u), filled(false)
    {
        std::memset(buf, 0, sizeof(buf));
    }

    void push(const float sample[INPUT_SIZE])
    {
        std::memcpy(buf[idx], sample, sizeof(float) * INPUT_SIZE);
        idx = (idx + 1u) % SEQ_LEN;
        if (idx == 0u) filled = true;
    }

    // 顺序导出窗口（时间顺序：旧 → 新）
    void export_seq(float dst[SEQ_LEN][INPUT_SIZE]) const
    {
        for (uint32_t t = 0; t < SEQ_LEN; ++t)
        {
            uint32_t src = (idx + t) % SEQ_LEN;
            std::memcpy(dst[t], buf[src], sizeof(float) * INPUT_SIZE);
        }
    }
};


class SOC_Estimator
{
public:
    SOC_Estimator()
        : soc_est  (0.f),
          P        (0.01f),
          cum_Ah   (CUM_AH_OFFSET),
          prev_V   (0.f),
          has_prev_V(false)
    {}

    // ────────────────────────────────
    //  初始化：拷贝权重 / 清零状态
    // ────────────────────────────────
    void init(void)
    {
        /* ------- 1) StandardScaler ------- */
        for (uint32_t i = 0; i < INPUT_SIZE; ++i)
        {
            scaler.mean [i] = scaler_mean [i];
            scaler.scale[i] = scaler_scale[i];
        }

        /* ------- 2) LSTM 层 0（4 维输入） ------- */
        net.layers[0].in_size = INPUT_SIZE;
        net.layers[0].W_ih = lstm0_W_ih;
        net.layers[0].W_hh = lstm0_W_hh;
        net.layers[0].b_ih = lstm0_b_ih;
        net.layers[0].b_hh = lstm0_b_hh;

        /* ------- 3) LSTM 层 1（64 维输入） ------- */
        net.layers[1].in_size = HIDDEN_SIZE;
        net.layers[1].W_ih = lstm1_W_ih;
        net.layers[1].W_hh = lstm1_W_hh;
        net.layers[1].b_ih = lstm1_b_ih;
        net.layers[1].b_hh = lstm1_b_hh;

        /* ------- 4) 全连接 FC ------- */
        net.fc_weight = fc_weight;
        net.fc_bias   = fc_bias;

        /* ------- 5) 清零历史状态 ------- */
        soc_est   = 0.f;
        P         = 0.01f;
        cum_Ah    = CUM_AH_OFFSET;
        prev_V    = 0.f;
        has_prev_V= false;
        window    = SlidingWindow();          // 重置滑窗
    }

    // ────────────────────────────────
    //  ☆ 新增：设置 SOC 起始值
    // ────────────────────────────────
    float set_initial_soc(float soc0)
    {
        soc_est = clip01(soc0);   // 剪裁到 [0,1]
        P       = 0.01f;          // 协方差重置
        return soc_est;
    }

    /** --------------------------------------------------------------------
     *  周期调用（一次采样）
     *
     *  @param I_A     电流  (A，充电:+  放电:-)
     *  @param V_V     电压  (V)
     *  @param dt_sec  与上采样间隔 (s)
     *  @return        估算 SOC (0~1)
     *  ------------------------------------------------------------------*/
    float update(float I_A, float V_V, float dt_sec)
    {
        // ───── 1) 衍生特征：dV/dt & 累计库伦量 ─────
        float dV_dt = 0.f;
        if (has_prev_V && dt_sec > 1e-6f)
            dV_dt = (V_V - prev_V) / dt_sec;   // V/s

        prev_V   = V_V;
        has_prev_V = true;

        // A·s → Ah（累加负号使放电为正量，便于直观）
        cum_Ah += -I_A * dt_sec / 3600.f;

        float raw [INPUT_SIZE] = { I_A, V_V, dV_dt, cum_Ah };
        float feat[INPUT_SIZE];
        scaler.transform(raw, feat);
        window.push(feat);

        // ───── 2) 纯库伦积分预测 (x_pred / P_pred) ─────
        float dt_h   = dt_sec / 3600.f;
        float delta  = (I_A >= 0.f)
                       ?  I_A * dt_h / (C_NOM * ETA_C)
                       : -std::fabs(I_A) * dt_h / (C_NOM * ETA_D);
        float x_pred = clip01(soc_est + delta);   // 边界裁剪
        float P_pred = P + Q_PROC;

        // ───── 3) LSTM + EKF 修正 (放电段 & 窗口已满) ─────
        float x_upd = x_pred, P_upd = P_pred;
        if (I_A < 0.f && window.filled)
        {
            float seq[SEQ_LEN][INPUT_SIZE];
            window.export_seq(seq);
            float z   = net.forward(seq);             // 网络输出
            float err = std::fabs(z - x_pred);

            if (err < ERR_TH)                         // 可信测量
            {
                float R_dyn = R_MEAS_BASE *
                              (1.f + std::pow(err / GATE, 4.f));
                ekf_update(x_pred, P_pred, z, R_dyn, x_upd, P_upd);
            }
        }

        soc_est = x_upd;
        P       = P_upd;
        return soc_est;
    }

private:
    StandardScaler scaler;
    LSTMNetwork    net;
    SlidingWindow  window;

    float soc_est;       // 状态估计
    float P;             // 协方差
    float cum_Ah;        // 已累积库伦量
    float prev_V;        // 上一帧电压
    bool  has_prev_V;    // 是否存在 prev_V
};

// ============================================================================
//  ↓↓↓                    C Compatible 外部接口区                        ↓↓↓
// ============================================================================
extern "C" {


static SOC_Estimator g_estimator;


void soc_estimator_init(void)
{
    g_estimator.init();
}


float soc_estimator_set_initial_soc(float init_soc)
{
    return g_estimator.set_initial_soc(init_soc);
}


float soc_estimator_update(float current_A,
                           float voltage_V,
                           float dt_sec)
{
    return g_estimator.update(current_A, voltage_V, dt_sec);
}

} // extern "C"

// =============================================================================
//  End of file
// =============================================================================



// =================== 桌面测试（可选） =================== //
#ifdef DESKTOP_TEST
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        puts("usage: ./ZincBromideBatterySOC  data.csv");
        return 0;
    }

    soc_estimator_init();

    const char* in_path  = argv[1];
    std::string out_path = std::string(in_path);
    size_t pos = out_path.rfind('.');
    if (pos != std::string::npos) out_path.insert(pos, "_pred");
    else                          out_path += "_pred.csv";

    std::ifstream fin(in_path);
    std::ofstream fout(out_path);
    if (!fin || !fout) { puts("❌ 文件打开失败"); return -1; }

    // 复制表头并追加 pred
    std::string head; std::getline(fin, head);
    fout << head << ",pred\n";

    float prev_t = 0.f; bool first = true;
    std::string line;
    while (std::getline(fin, line))
    {
        if (line.empty()) break;
        std::stringstream ss(line);
        std::vector<float> f; std::string tok;
        while (std::getline(ss, tok, ',')) f.push_back(std::stof(tok));

        float t  = f[0];          // 假设第一列时间(s)
        float I  = f[1];          // 第二列电流(A)
        float V  = f[2];          // 第三列电压(V)

        float dt = first ? 0.f : (t - prev_t);
        first = false;  prev_t = t;

        float pred = soc_estimator_update(I, V, dt);

        fout << std::fixed << std::setprecision(6)
             << t << ',' << I << ',' << V;
        for (size_t i = 3; i < f.size(); ++i) fout << ',' << f[i];
        fout << ',' << pred << '\n';
    }

    fin.close(); fout.close();
    printf("✓ 输出：%s\n", out_path.c_str());
    return 0;
}
#endif  // DESKTOP_TEST
