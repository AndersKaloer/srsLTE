/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2014 The libLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the libLTE library.
 *
 * libLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * A copy of the GNU Lesser General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include "liblte/config.h"

#include "liblte/phy/ch_estimation/chest_dl.h"
#include "liblte/phy/utils/vector.h"
#include "liblte/phy/utils/convolution.h"

#define CHEST_RS_AVERAGE_TIME   2
#define CHEST_RS_AVERAGE_FREQ   3

#define NOISE_POWER_USE_ESTIMATES


/** 3GPP LTE Downlink channel estimator and equalizer. 
 * Estimates the channel in the resource elements transmitting references and interpolates for the rest
 * of the resource grid. 
 * 
 * The equalizer uses the channel estimates to produce an estimation of the transmitted symbol. 
 * 
 * This object depends on the refsignal_t object for creating the LTE CSR signal.  
*/

int chest_dl_init(chest_dl_t *q, lte_cell_t cell) 
{
  int ret = LIBLTE_ERROR_INVALID_INPUTS;
  if (q                != NULL &&
      lte_cell_isvalid(&cell)) 
  {
    bzero(q, sizeof(chest_dl_t));
    
    ret = refsignal_cs_generate(&q->csr_signal, cell); 
    if (ret != LIBLTE_SUCCESS) {
      fprintf(stderr, "Error initializing CSR signal (%d)\n",ret);
      goto clean_exit;
    }
    
    q->tmp_freqavg = vec_malloc(sizeof(cf_t) * REFSIGNAL_MAX_NUM_SF(cell.nof_prb));
    if (!q->tmp_freqavg) {
      perror("malloc");
      goto clean_exit;
    }
    q->tmp_noise = vec_malloc(sizeof(cf_t) * REFSIGNAL_MAX_NUM_SF(cell.nof_prb));
    if (!q->tmp_noise) {
      perror("malloc");
      goto clean_exit;
    }
    for (int i=0;i<CHEST_MAX_FILTER_TIME_LEN;i++) {
      q->tmp_timeavg[i] = vec_malloc(sizeof(cf_t) * 2*cell.nof_prb);
      if (!q->tmp_timeavg[i]) {
        perror("malloc");
        goto clean_exit;
      }
      bzero(q->tmp_timeavg[i], sizeof(cf_t) * 2*cell.nof_prb);
    }
    
    for (int i=0;i<cell.nof_ports;i++) {
      q->pilot_estimates[i] = vec_malloc(sizeof(cf_t) * REFSIGNAL_NUM_SF(cell.nof_prb, i));
      if (!q->pilot_estimates[i]) {
        perror("malloc");
        goto clean_exit;
      }      
      q->pilot_estimates_average[i] = vec_malloc(sizeof(cf_t) * REFSIGNAL_NUM_SF(cell.nof_prb, i));
      if (!q->pilot_estimates_average[i]) {
        perror("malloc");
        goto clean_exit;
      }      
      q->pilot_recv_signal[i] = vec_malloc(sizeof(cf_t) * REFSIGNAL_NUM_SF(cell.nof_prb, i));
      if (!q->pilot_recv_signal[i]) {
        perror("malloc");
        goto clean_exit;
      }
    }
    
    if (interp_linear_vector_init(&q->interp_linvec, RE_X_RB*cell.nof_prb)) {
      fprintf(stderr, "Error initializing vector interpolator\n");
      goto clean_exit; 
    }

    if (interp_linear_init(&q->interp_lin, 2*cell.nof_prb, RE_X_RB/2)) {
      fprintf(stderr, "Error initializing interpolator\n");
      goto clean_exit; 
    }
    
    /* Set default time/freq filters */
    //float f[3]={0.1, 0.8, 0.1};
    //chest_dl_set_filter_freq(q, f, 3);

    float f[5]={0.05, 0.15, 0.6, 0.15, 0.05};
    chest_dl_set_filter_freq(q, f, 5);
    
    float t[2]={0.1, 0.9};
    chest_dl_set_filter_time(q, t, 0);
    
    q->cell = cell; 
  }
  
  ret = LIBLTE_SUCCESS;
  
clean_exit:
  if (ret != LIBLTE_SUCCESS) {
      chest_dl_free(q);
  }
  return ret; 
}

void chest_dl_free(chest_dl_t *q) 
{
  refsignal_cs_free(&q->csr_signal);

  if (q->tmp_freqavg) {
    free(q->tmp_freqavg);
  }
  if (q->tmp_noise) {
    free(q->tmp_noise);
  }
  for (int i=0;i<CHEST_MAX_FILTER_TIME_LEN;i++) {
    if (q->tmp_timeavg[i]) {
      free(q->tmp_timeavg[i]);
    }
  }
  interp_linear_vector_free(&q->interp_linvec);
  interp_linear_free(&q->interp_lin);
  
  for (int i=0;i<MAX_PORTS;i++) {
    if (q->pilot_estimates[i]) {
      free(q->pilot_estimates[i]);
    }      
    if (q->pilot_estimates_average[i]) {
      free(q->pilot_estimates_average[i]);
    }      
    if (q->pilot_recv_signal[i]) {
      free(q->pilot_recv_signal[i]);
    }
  }
  bzero(q, sizeof(chest_dl_t));
}

int chest_dl_set_filter_freq(chest_dl_t *q, float *filter, uint32_t filter_len) {
  if (filter_len <= CHEST_MAX_FILTER_FREQ_LEN) {
    q->filter_freq_len = filter_len; 
    for (int i=0;i<filter_len;i++) {
      q->filter_freq[i] = filter[i];
    }
    return LIBLTE_SUCCESS;
  } else {
    return LIBLTE_ERROR;
  }
}

int chest_dl_set_filter_time(chest_dl_t *q, float *filter, uint32_t filter_len) {
  if (filter_len <= CHEST_MAX_FILTER_TIME_LEN) {
    q->filter_time_len = filter_len; 
    for (int i=0;i<filter_len;i++) {
      q->filter_time[i] = filter[i];
    }    
    return LIBLTE_SUCCESS;
  } else {
    return LIBLTE_ERROR;
  }  
}



#ifdef NOISE_POWER_USE_ESTIMATES

/* Uses the difference between the averaged and non-averaged pilot estimates */
static float estimate_noise_port(chest_dl_t *q, uint32_t port_id, cf_t *avg_pilots) {
  /* Use difference between averaged and noisy LS pilot estimates */
  vec_sub_ccc(avg_pilots, q->pilot_estimates[port_id],
              q->tmp_noise, REFSIGNAL_NUM_SF(q->cell.nof_prb, port_id));

  return vec_avg_power_cf(q->tmp_noise, REFSIGNAL_NUM_SF(q->cell.nof_prb, port_id));
}

#else

/* Uses the 5 empty transmitted SC before and after the SSS and PSS sequences for noise estimation */
static float estimate_noise_empty_sc(chest_dl_t *q, cf_t *input) {
  int k_sss = (CP_NSYMB(q->cell.cp) - 2) * q->cell.nof_prb * RE_X_RB + q->cell.nof_prb * RE_X_RB / 2 - 31;
  float noise_power = 0; 
  noise_power += vec_avg_power_cf(&input[k_sss-5], 5); // 5 empty SC before SSS
  noise_power += vec_avg_power_cf(&input[k_sss+62], 5); // 5 empty SC after SSS
  int k_pss = (CP_NSYMB(q->cell.cp) - 1) * q->cell.nof_prb * RE_X_RB + q->cell.nof_prb * RE_X_RB / 2 - 31;
  noise_power += vec_avg_power_cf(&input[k_pss-5], 5); // 5 empty SC before PSS
  noise_power += vec_avg_power_cf(&input[k_pss+62], 5); // 5 empty SC after PSS
  
  return noise_power; 
}
#endif

#define pilot_est(idx) q->pilot_estimates[port_id][REFSIGNAL_PILOT_IDX(idx,l,q->cell)]
#define pilot_avg(idx) q->pilot_estimates_average[port_id][REFSIGNAL_PILOT_IDX(idx,l,q->cell)]
#define pilot_tmp(idx) q->tmp_freqavg[REFSIGNAL_PILOT_IDX(idx,l,q->cell)]

static void average_pilots(chest_dl_t *q, uint32_t port_id) 
{
  int nref=2*q->cell.nof_prb;
  uint32_t l, i;

  /* For each symbol with pilots in a slot */
  for (l=0;l<refsignal_cs_nof_symbols(port_id);l++) {
    if (q->filter_freq_len > 0) {
      /* Filter pilot estimates in frequency */
      conv_same_cf(&pilot_est(0), q->filter_freq, &pilot_tmp(0), nref, q->filter_freq_len);
      
      /* Adjust extremes using linear interpolation */
      pilot_tmp(0) += interp_linear_onesample(pilot_est(1), pilot_est(0)) 
                        * q->filter_freq[q->filter_freq_len/2-1];
      pilot_tmp(nref-1) += interp_linear_onesample(pilot_est(nref-2), pilot_est(nref-1)) 
                        * q->filter_freq[q->filter_freq_len/2+1];        
    } else {
      memcpy(&pilot_tmp(0), &pilot_est(0), nref * sizeof(cf_t));
    }
  }

  #ifdef NOISE_POWER_USE_ESTIMATES
  q->noise_estimate[port_id] = estimate_noise_port(q, port_id, q->tmp_freqavg);
  #endif
  
  for (l=0;l<refsignal_cs_nof_symbols(port_id);l++) {
    /* Filter in time domain. */
    if (q->filter_time_len > 0) {
      /* Move last symbols */
      for (i=0;i<q->filter_time_len-1;i++) {
        memcpy(q->tmp_timeavg[i], q->tmp_timeavg[i+1], nref*sizeof(cf_t));                      
      }
      /* Put last symbol to buffer */
      memcpy(q->tmp_timeavg[i], &pilot_tmp(0), nref*sizeof(cf_t));            

      /* Multiply all symbols by filter and add them  */
      bzero(&pilot_avg(0), nref * sizeof(cf_t));
      for (i=0;i<q->filter_time_len;i++) {
        vec_sc_prod_cfc(q->tmp_timeavg[i], q->filter_time[i], q->tmp_timeavg[i], nref);
        vec_sum_ccc(q->tmp_timeavg[i], &pilot_avg(0), &pilot_avg(0), nref);            
      }
    } else {
      memcpy(&pilot_avg(0), &pilot_tmp(0), nref * sizeof(cf_t));        
    }
  }
  
}

#define cesymb(i) ce[RE_IDX(q->cell.nof_prb,i,0)]

static void interpolate_pilots(chest_dl_t *q, cf_t *ce, uint32_t port_id) 
{
  /* interpolate the symbols with references in the freq domain */
  uint32_t l; 
  uint32_t nsymbols = refsignal_cs_nof_symbols(port_id); 
  
  /* Interpolate in the frequency domain */
  for (l=0;l<nsymbols;l++) {
    uint32_t fidx_offset = refsignal_fidx(q->cell, l, port_id, 0);    
    interp_linear_offset(&q->interp_lin, &pilot_avg(0),
                         &ce[refsignal_nsymbol(l,q->cell.cp, port_id) * q->cell.nof_prb * RE_X_RB], 
                         fidx_offset, RE_X_RB/2-fidx_offset); 
  }
  
  /* Now interpolate in the time domain between symbols */
  if (CP_ISNORM(q->cell.cp)) {
    if (nsymbols == 4) {
      interp_linear_vector(&q->interp_linvec, &cesymb(0), &cesymb(4), &cesymb(1), 3);
      interp_linear_vector(&q->interp_linvec, &cesymb(4), &cesymb(7), &cesymb(5), 2);
      interp_linear_vector(&q->interp_linvec, &cesymb(7), &cesymb(11), &cesymb(8), 3);
      interp_linear_vector(&q->interp_linvec, &cesymb(7), &cesymb(11), &cesymb(12), 2);
    } else {
      interp_linear_vector(&q->interp_linvec, &cesymb(8), &cesymb(1), &cesymb(0), 1);
      interp_linear_vector(&q->interp_linvec, &cesymb(1), &cesymb(8), &cesymb(2), 6);
      interp_linear_vector(&q->interp_linvec, &cesymb(1), &cesymb(8), &cesymb(9), 5);
    }    
  } else {
    if (nsymbols == 4) {
      interp_linear_vector(&q->interp_linvec, &cesymb(0), &cesymb(3), &cesymb(1), 2);
      interp_linear_vector(&q->interp_linvec, &cesymb(3), &cesymb(6), &cesymb(4), 2);
      interp_linear_vector(&q->interp_linvec, &cesymb(6), &cesymb(9), &cesymb(7), 2);
      interp_linear_vector(&q->interp_linvec, &cesymb(6), &cesymb(9), &cesymb(9), 2);
    } else {
      interp_linear_vector(&q->interp_linvec, &cesymb(7), &cesymb(1), &cesymb(0), 1);
      interp_linear_vector(&q->interp_linvec, &cesymb(1), &cesymb(7), &cesymb(2), 5);
      interp_linear_vector(&q->interp_linvec, &cesymb(1), &cesymb(7), &cesymb(8), 4);
    }    
  }
}

float chest_dl_rssi(chest_dl_t *q, cf_t *input, uint32_t port_id) {
  uint32_t l;
  
  float rssi = 0;
  uint32_t nsymbols = refsignal_cs_nof_symbols(port_id);   
  for (l=0;l<nsymbols;l++) {
    cf_t *tmp = &input[refsignal_nsymbol(l, q->cell.cp, port_id) * q->cell.nof_prb * RE_X_RB];
    rssi += vec_dot_prod_conj_ccc(tmp, tmp, q->cell.nof_prb * RE_X_RB);    
  }    
  return rssi/nsymbols; 
}

//#define RSRP_FROM_ESTIMATES

float chest_dl_rsrp(chest_dl_t *q, uint32_t port_id) {
#ifdef RSRP_FROM_ESTIMATES
  return vec_avg_power_cf(q->pilot_estimates[port_id], 
                          REFSIGNAL_NUM_SF(q->cell.nof_prb, port_id));
#else
  return vec_avg_power_cf(q->pilot_estimates_average[port_id], 
                          REFSIGNAL_NUM_SF(q->cell.nof_prb, port_id));
#endif
}

int chest_dl_estimate_port(chest_dl_t *q, cf_t *input, cf_t *ce, uint32_t sf_idx, uint32_t port_id) 
{
  /* Get references from the input signal */
  refsignal_cs_get_sf(q->cell, port_id, input, q->pilot_recv_signal[port_id]);
  
  /* Use the known CSR signal to compute Least-squares estimates */
  vec_prod_conj_ccc(q->pilot_recv_signal[port_id], q->csr_signal.pilots[port_id/2][sf_idx], 
              q->pilot_estimates[port_id], REFSIGNAL_NUM_SF(q->cell.nof_prb, port_id)); 

  /* Average pilot estimates */
  average_pilots(q, port_id);

  /* Compute RSRP for the channel estimates in this port */
  q->rsrp[port_id] = chest_dl_rsrp(q, port_id);     
  if (port_id == 0) {
    /* compute rssi only for port 0 */
    q->rssi[port_id] = chest_dl_rssi(q, input, port_id);     
  }
      
  /* Interpolate to create channel estimates for all resource grid */
  if (ce != NULL) {
    interpolate_pilots(q, ce, port_id);    
  }
  
#ifndef NOISE_POWER_USE_ESTIMATES
  q->noise_estimate[port_id] = estimate_noise_empty_sc(q, input);
#endif
  return 0;
}

int chest_dl_estimate(chest_dl_t *q, cf_t *input, cf_t *ce[MAX_PORTS], uint32_t sf_idx) 
{
  uint32_t port_id; 
  
  for (port_id=0;port_id<q->cell.nof_ports;port_id++) {
    chest_dl_estimate_port(q, input, ce[port_id], sf_idx, port_id);
  }
  return LIBLTE_SUCCESS;
}

float chest_dl_get_noise_estimate(chest_dl_t *q) {
  float noise = vec_acc_ff(q->noise_estimate, q->cell.nof_ports)/q->cell.nof_ports;
#ifdef NOISE_POWER_USE_ESTIMATES
  return noise*sqrtf(lte_symbol_sz(q->cell.nof_prb));
#else
  return noise; 
#endif
  
}

float chest_dl_get_snr(chest_dl_t *q) {
  // Uses RSRP as an estimation of the useful signal power  
  return chest_dl_get_rsrp(q)/chest_dl_get_noise_estimate(q)/sqrt(2)/q->cell.nof_ports;
}

float chest_dl_get_rssi(chest_dl_t *q) {
  return 4*q->rssi[0]/q->cell.nof_prb/RE_X_RB; 
}

/* q->rssi[0] is the average power in all RE in all symbol containing references for port 0 . q->rssi[0]/q->cell.nof_prb is the average power per PRB 
 * q->rsrp[0] is the average power of RE containing references only (for port 0). 
*/ 
float chest_dl_get_rsrq(chest_dl_t *q) {
  return q->cell.nof_prb*q->rsrp[0] / q->rssi[0];
  
}

float chest_dl_get_rsrp(chest_dl_t *q) {
  
  // return sum of power received from all tx ports
  return vec_acc_ff(q->rsrp, q->cell.nof_ports); 
}

