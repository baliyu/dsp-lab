/* dsp.c compiled with function-level -O2 (#pragma GCC optimize) under other names (dsp_o2_...), so that one build can time the same code at the build's default
   optimisation level and at -O2. The pragma is not identical to compiling with -O2 on the command line (it does not change every compiler setting). */
#pragma GCC optimize ("O2")
#define dsp_biquad_init dsp_o2_biquad_init
#define dsp_biquad_step dsp_o2_biquad_step
#define dsp_biquad_cascade dsp_o2_biquad_cascade
#define dsp_fir_init dsp_o2_fir_init
#define dsp_fir_step dsp_o2_fir_step
#define dsp_fir dsp_o2_fir
#include "dsp.c"
