/* dsp32.c compiled with function-level -O2 under other names (dsp32_o2_...); see dsp_o2.c. */
#pragma GCC optimize ("O2")
#define dsp32_biquad_ok dsp32_o2_biquad_ok
#define dsp32_fir_ok dsp32_o2_fir_ok
#define dsp32_biquad_step dsp32_o2_biquad_step
#define dsp32_biquad_cascade dsp32_o2_biquad_cascade
#define dsp32_fir_step dsp32_o2_fir_step
#define dsp32_fir dsp32_o2_fir
#define dsp32_fir_sym_ok dsp32_o2_fir_sym_ok
#define dsp32_biquad_blk dsp32_o2_biquad_blk
#define dsp32_fir_lin dsp32_o2_fir_lin
#define dsp32_fir_sym dsp32_o2_fir_sym
#include "dsp32.c"
