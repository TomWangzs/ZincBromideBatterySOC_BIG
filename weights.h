#ifndef WEIGHTS_H_
#define WEIGHTS_H_
#include <cstdint>

extern const float scaler_mean[4];
extern const float scaler_scale[4];
extern const float lstm0_W_ih[1024];
extern const float lstm0_W_hh[16384];
extern const float lstm0_b_ih[256];
extern const float lstm0_b_hh[256];
extern const float lstm1_W_ih[16384];
extern const float lstm1_W_hh[16384];
extern const float lstm1_b_ih[256];
extern const float lstm1_b_hh[256];
extern const float fc_weight[64];
extern const float fc_bias;
#endif /* WEIGHTS_H_ */
