/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <complex.h>

#include "srslte/cuhd/cuhd.h"
#include "srslte/srslte.h"

#define MAX_LEN  70176


uint32_t nof_prb          = 25;
uint32_t preamble_format  = 0;
uint32_t root_seq_idx     = 0;
uint32_t seq_idx          = 0;
uint32_t frequency_offset = 0;
uint32_t zero_corr_zone   = 0;
uint32_t nof_frames = 20; 

float uhd_gain=40, uhd_freq=2.4e9; 
char *uhd_args="";
char *output_filename = NULL;

void usage(char *prog) {
  printf("Usage: %s \n", prog);
  printf("\t-a UHD args [Default %s]\n", uhd_args);
  printf("\t-f UHD TX/RX frequency [Default %.2f MHz]\n", uhd_freq/1e6);
  printf("\t-g UHD TX/RX gain [Default %.1f dB]\n", uhd_gain);
  printf("\t-p Number of UL RB [Default %d]\n", nof_prb);
  printf("\t-F Preamble format [Default %d]\n", preamble_format);
  printf("\t-s sequence index [Default %d]\n", seq_idx);
  printf("\t-r Root sequence index [Default %d]\n", root_seq_idx);
  printf("\t-z Zero correlation zone config [Default %d]\n", zero_corr_zone);
  printf("\t-o Save transmitted PRACH in file [Default no]\n");
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "apfFgrsoz")) != -1) {
    switch (opt) {
    case 'a':
      uhd_args = argv[optind];
      break;
    case 'o':
      output_filename = argv[optind];
      break;
    case 'f':
      uhd_freq = atof(argv[optind]);
      break;
    case 'g':
      uhd_gain = atof(argv[optind]);
      break;
    case 'p':
      nof_prb = atoi(argv[optind]);
      if (!srslte_nofprb_isvalid(nof_prb)) {
        fprintf(stderr, "Invalid number of UL RB %d\n", nof_prb);
        exit(-1);
      }
      break;
    case 'F':
      preamble_format = atoi(argv[optind]);
      break;
    case 'r':
      root_seq_idx = atoi(argv[optind]);
      break;
    case 's':
      seq_idx = atoi(argv[optind]);
      break;
    case 'z':
      zero_corr_zone = atoi(argv[optind]);
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
}

int main(int argc, char **argv) {
  parse_args(argc, argv);

  srslte_prach_t *p = (srslte_prach_t*)malloc(sizeof(srslte_prach_t));

  bool high_speed_flag      = false;

  cf_t preamble[MAX_LEN];
  memset(preamble, 0, sizeof(cf_t)*MAX_LEN);

  srslte_prach_init(p,
             srslte_symbol_sz(nof_prb),
             preamble_format,
             root_seq_idx,
             high_speed_flag,
             zero_corr_zone);

  uint32_t flen = srslte_sampling_freq_hz(nof_prb)/1000;

  printf("Generating PRACH\n");
  bzero(preamble, flen*sizeof(cf_t));
  srslte_prach_gen(p,
            seq_idx,
            frequency_offset,
            preamble);
  
  
  uint32_t prach_len = p->N_seq;
  
  srslte_vec_save_file("generated",preamble,prach_len*sizeof(cf_t));
  
  cf_t *buffer = malloc(sizeof(cf_t)*flen*nof_frames);
  
  // Send through UHD 
  void *uhd; 
  printf("Opening UHD device...\n");
  if (cuhd_open(uhd_args, &uhd)) {
    fprintf(stderr, "Error opening uhd\n");
    exit(-1);
  }
  printf("Subframe len:   %d samples\n", flen);
  printf("Set TX/RX rate: %.2f MHz\n", cuhd_set_rx_srate(uhd, srslte_sampling_freq_hz(nof_prb)) / 1000000);
  printf("Set RX gain: %.1f dB\n", cuhd_set_rx_gain(uhd, uhd_gain));
  printf("Set TX gain: %.1f dB\n", cuhd_set_tx_gain(uhd, uhd_gain));
  printf("Set TX/RX freq: %.2f MHz\n", cuhd_set_rx_freq(uhd, uhd_freq) / 1000000);
  cuhd_set_tx_srate(uhd, srslte_sampling_freq_hz(nof_prb));
  cuhd_set_tx_freq_offset(uhd, uhd_freq, 8e6);  
  sleep(1);
  
  cf_t *zeros = calloc(sizeof(cf_t),flen);
  
  FILE *f = NULL; 
  if (output_filename) {        
    f = fopen(output_filename, "w");
  }
  
  srslte_timestamp_t tstamp; 
  
  cuhd_start_rx_stream(uhd);
  uint32_t nframe=0;
  
  while(nframe<nof_frames) {
    printf("Rx subframe %d\n", nframe);
    cuhd_recv_with_time(uhd, &buffer[flen*nframe], flen, true, &tstamp.full_secs, &tstamp.frac_secs);
    nframe++;
    if (nframe==9 || nframe==8) {
      srslte_timestamp_add(&tstamp, 0, 2e-3);
      if (nframe==8) {
        cuhd_send_timed2(uhd, zeros, flen, tstamp.full_secs, tstamp.frac_secs, true, false);      
        printf("Transmitting zeros\n");        
      } else {
        cuhd_send_timed2(uhd, preamble, flen, tstamp.full_secs, tstamp.frac_secs, true, true);      
        printf("Transmitting PRACH\n");      
      }
    }

  }
  if (f) {
    fwrite(&buffer[10*flen], flen*sizeof(cf_t), 1, f);
  }
  if (f) {
    fclose(f);
  }

  srslte_prach_free(p);
  free(p);

  printf("Done\n");
  exit(0);
}
