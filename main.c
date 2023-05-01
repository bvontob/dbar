#include "userosc.h"

#define DBAR_NUM 9

#define AMP_MAX   0.2f
#define AMP_0     0.0f

#define RECOMP_F -1.0f

enum dbar_reg {
  DBAR_REG_A = 0,
  DBAR_REG_B = 1,
  DBAR_REG_W = 2,
  DBAR_REGS
};

static int   dbar_sel    = DBAR_NUM;
static int   perc        = 0;
static int   perc_dbar   = -1;
static float reg_mix     = 0.0f;
static float amp_sum     = 0.0f;
static float dirt        = 0.0f;
static float noise_level = RECOMP_F;

static int count = 0;
static float perc_amp = 0.0f;
static float noise = 0.0f;
static float phi[DBAR_NUM] = { 0.0f };
static float w[DBAR_NUM];

static const uint8_t dbar_notes[DBAR_NUM] = {
  -12, // 16'
  7,   // 5 1/3'
  0,   // 8'
  12,  // 4'
  19,  // 2 2/3'
  24,  // 2'
  28,  // 1 3/5'
  31,  // 1 1/3'
  36   // 1'
};

static float amp[DBAR_REGS][DBAR_NUM] = {
  {
    AMP_MAX, // 16'
    AMP_MAX, // 5 1/3'
    AMP_MAX, // 8'
    AMP_0,   // 4'
    AMP_0,   // 2 2/3'
    AMP_0,   // 2'
    AMP_0,   // 1 3/5'
    AMP_0,   // 1 1/3'
    AMP_0    // 1'
  }, {
    AMP_MAX, // 16'
    AMP_MAX, // 5 1/3'
    AMP_MAX, // 8'
    AMP_MAX, // 4'
    AMP_MAX, // 2 2/3'
    AMP_MAX, // 2'
    AMP_MAX, // 1 3/5'
    AMP_MAX, // 1 1/3'
    AMP_MAX  // 1'
  }, {
    AMP_0,   // 16'
    AMP_0,   // 5 1/3'
    AMP_0,   // 8'
    AMP_0,   // 4'
    AMP_0,   // 2 2/3'
    AMP_0,   // 2'
    AMP_0,   // 1 3/5'
    AMP_0,   // 1 1/3'
    AMP_0    // 1'
  }
};

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames) {
  const uint8_t note  = (params->pitch) >> 8;
  const uint8_t mod   = (params->pitch) & 0x00FF;

  for (int i = 0; i < DBAR_NUM; i++)
    w[i] = osc_w0f_for_note(note + dbar_notes[i], mod);

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;
  
  for (; y != y_e; ) {
    float sig = 0;
    for (int i = 0; i < DBAR_NUM; i++) {
      sig += amp[DBAR_REG_W][i] * osc_sinf(phi[i]); // osc_bl2_parf(phi[i], 0);

      if (i == perc_dbar) {
	sig += perc_amp * osc_sinf(phi[i]);
	if (perc_amp > 0.0f)
	  perc_amp -= (perc & 0x04) ? 0.00001f : 0.0001f;
      }
      
      phi[i] += w[i];
      phi[i] -= (uint32_t)phi[i];
    }

    // Downsampled noise
    if (count++ == 3) {
      count = 0;
      noise = noise_level * _osc_white();
    }
    
    sig += noise;
    sig  = osc_sat_schetzenf(sig);

    *(y++) = f32_to_q31(sig);
  }
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  (void)params;
  perc_amp = (perc & 0x01) ? 0.1f : 0.3f;
}

void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
}

void OSC_PARAM(uint16_t idx, uint16_t val) { 
  switch (idx) {

  case k_user_osc_param_id1:
    dbar_sel = val;
    break;

  case k_user_osc_param_id2:
    /*
     *  -1: percussion off
     *  bit 0 (0x01): soft (1) vs normal
     *  bit 1 (0x02): 4' (1) vs 2 2/3'
     *  bit 2 (0x04): slow (1) vs fast
     */
    perc = (int)val - 1;
    perc_dbar = perc < 0 ? -1
      : ((perc & 0x02) ? 4 : 5);
    break;

  case k_user_osc_param_id3:
    dirt = 0.01f * (float)val;
    noise_level = RECOMP_F;
    break;

  case k_user_osc_param_shape:
    if (dbar_sel < DBAR_NUM)
      amp[reg_mix < 0.5f ? DBAR_REG_A : DBAR_REG_B][dbar_sel] =
	param_val_to_f32(val) * AMP_MAX;
    else
      reg_mix = param_val_to_f32(val);
    amp_sum = 0.0f;
    for (int i = 0; i < DBAR_NUM; i++) {
      amp[DBAR_REG_W][i] =
	((1.0f - reg_mix) * amp[DBAR_REG_A][i] + reg_mix * amp[DBAR_REG_B][i]);
      amp_sum += amp[DBAR_REG_W][i];
    }
    noise_level = RECOMP_F;
    break;

  case k_user_osc_param_shiftshape:
    (void)param_val_to_f32(val);
    break;
    
  default:
    break;
  }

  if (noise_level < 0)
    noise_level = dirt * amp_sum * 0.01f;
}
