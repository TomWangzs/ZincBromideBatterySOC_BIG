/******************************************************************************
 *  hybrid_cc_lstm_ekf_soc.h -- C 接口头文件                                  *
 *                                                                            *
 *  估算对象：锌溴液流电池 SOC                                                 *
 *  适用编译器：ARM Compiler 5（--cpp11 --no_rtti --no_exceptions）             *
 *  依赖文件：                                                                 *
 *      · hybrid_cc_lstm_ekf_soc_v3_ac5.cpp  -- 实现                           *
 *      · weights.h                       -- 模型与 StandardScaler 权重        *
 ******************************************************************************/

#ifndef HYBRID_CC_LSTM_EKF_SOC_H_
#define HYBRID_CC_LSTM_EKF_SOC_H_

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 *  初始化
 *---------------------------------------------------------------------------
 *  功能：   载入权重并清零内部状态。若不额外设置，SOC 起点默认为 0。
 *  调用时机：上电或 MCU 复位后，仅调用一次。
 */
void  soc_estimator_init(void);

/*---------------------------------------------------------------------------
 *  设置初始 SOC  (可选)
 *---------------------------------------------------------------------------
 *  @param init_soc  0 ~ 1 之间；实参会被裁剪到该区间
 *  @return          实际生效的 SOC（裁剪后）
 *  备注：           必须在第一次 soc_estimator_update() 之前调用
 */
float soc_estimator_set_initial_soc(float init_soc);

/*---------------------------------------------------------------------------
 *  在线更新
 *---------------------------------------------------------------------------
 *  @param current_A  实时电流  (A)    —— 充电为正，放电为负
 *  @param voltage_V  实时电压  (V)
 *  @param dt_sec     距离上一次调用的时间间隔 (秒)
 *  @return           估算得到的 SOC，范围 0 ~ 1
 *  调用频率：        与采样周期一致，例如 10 Hz / 100 ms
 */
float soc_estimator_update(float current_A,
                           float voltage_V,
                           float dt_sec);

#ifdef __cplusplus
}
#endif

#endif /* HYBRID_CC_LSTM_EKF_SOC_H_ */
