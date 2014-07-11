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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include "liblte/phy/phy.h"
#include "liblte/cuhd/cuhd.h"
void *uhd;


#define MHZ           1000000
#define SAMP_FREQ     1920000
#define FLEN          9600
#define FLEN_PERIOD   0.005

#define NOF_PORTS 2
#define NOF_PRACH_SEQUENCES 52

float find_threshold = 20.0, track_threshold = 10.0;
int max_track_lost = 20, nof_frames = -1;
int track_len=300;
char *input_file_name = NULL;
int disable_plots = 0;
double pss_time_offset = (6/14)*10e-3;
double prach_time_offset = 4*10e-3; //Subframe 4

int go_exit=0;

double uhd_rx_freq = 2680000000.0, uhd_rx_gain = 40.0;
double uhd_rx_offset = 0.0;
double uhd_tx_freq = 2560000000.0, uhd_tx_gain = 50.0, uhd_tx_rate = 7680000.0;
double uhd_tx_offset = 8000000;
char *uhd_args = "";

filesource_t fsrc;
cf_t *input_buffer, *fft_buffer, *ce[MAX_PORTS_CTRL];
pbch_t pbch;
lte_fft_t fft;
chest_t chest;
sync_t sfind, strack;
cfo_t cfocorr;
prach_t prach;
cf_t *prach_buffers[NOF_PRACH_SEQUENCES];
int prach_buffer_len;


enum sync_state {FIND, TRACK};

void usage(char *prog) {
  printf("Usage: %s [iagfndvp]\n", prog);
  printf("\t-i input_file [Default use USRP]\n");
  printf("\t-a UHD args [Default %s]\n", uhd_args);
  printf("\t-g UHD RX gain [Default %.2f dB]\n", uhd_rx_gain);
  printf("\t-f UHD RX frequency [Default %.1f MHz]\n", uhd_rx_freq/1000000);
  printf("\t-n nof_frames [Default %d]\n", nof_frames);
  printf("\t-p PSS threshold [Default %f]\n", find_threshold);
  printf("\t-v [set verbosity - 0=none, 1=info, 2=debug - Default 0]\n");
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "iagfnvp")) != -1) {
    switch(opt) {
    case 'i':
      input_file_name = argv[optind];
      break;
    case 'a':
      uhd_args = argv[optind];
      break;
    case 'g':
      uhd_rx_gain = atof(argv[optind]);
      break;
    case 'f':
      uhd_rx_freq = atof(argv[optind]);
      break;
    case 'p':
      find_threshold = atof(argv[optind]);
      break;
    case 'n':
      nof_frames = atoi(argv[optind]);
      break;
    case 'v':
      verbose = atoi(argv[optind]);
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
}

int base_init(int frame_length) {
  int i;

  if (input_file_name) {
    if (filesource_init(&fsrc, input_file_name, COMPLEX_FLOAT_BIN)) {
      return -1;
    }
  } else {
    /* open UHD devices */
    printf("Opening UHD rx device...\n");
    if (cuhd_open(uhd_args,&uhd)) {
      fprintf(stderr, "Error opening uhd rx\n");
      return -1;
    }
  }

  input_buffer = (cf_t*) malloc(frame_length * sizeof(cf_t));
  if (!input_buffer) {
    perror("malloc");
    return -1;
  }

  fft_buffer = (cf_t*) malloc(CPNORM_NSYMB * 72 * sizeof(cf_t));
  if (!fft_buffer) {
    perror("malloc");
    return -1;
  }

  for (i=0;i<MAX_PORTS_CTRL;i++) {
    ce[i] = (cf_t*) malloc(CPNORM_NSYMB * 72 * sizeof(cf_t));
    if (!ce[i]) {
      perror("malloc");
      return -1;
    }
  }
  if (sync_init(&sfind, FLEN)) {
    fprintf(stderr, "Error initiating PSS/SSS\n");
    return -1;
  }
  if (sync_init(&strack, track_len)) {
    fprintf(stderr, "Error initiating PSS/SSS\n");
    return -1;
  }
  if (chest_init(&chest, LINEAR, CPNORM, 6, NOF_PORTS)) {
    fprintf(stderr, "Error initializing equalizer\n");
    return -1;
  }

  if (cfo_init(&cfocorr, FLEN)) {
    fprintf(stderr, "Error initiating CFO\n");
    return -1;
  }

  if (lte_fft_init(&fft, CPNORM, 6)) {
    fprintf(stderr, "Error initializing FFT\n");
    return -1;
  }

  if (prach_init(&prach, 512, 0, 648, false, 11)) {
    fprintf(stderr, "Error initializing PRACH\n");
    return -1;
  }
  prach_buffer_len = prach.N_seq + prach.N_cp;
  for(int i=0;i<NOF_PRACH_SEQUENCES;i++){
    prach_buffers[i] = (cf_t*)malloc(prach_buffer_len*sizeof(cf_t));
    if(!prach_buffers[i]) {
      perror("maloc");
      return -1;
    }
  }

  return 0;
}

void base_free() {
  int i;

  if (input_file_name) {
    filesource_free(&fsrc);
  } else {
    cuhd_close(uhd);
  }

  sync_free(&sfind);
  sync_free(&strack);
  lte_fft_free(&fft);
  chest_free(&chest);
  cfo_free(&cfocorr);
  prach_free(&prach);

  for(int i=0;i<NOF_PRACH_SEQUENCES;i++){
    free(prach_buffers[i]);
  }
  free(input_buffer);
  free(fft_buffer);
  for (i=0;i<MAX_PORTS_CTRL;i++) {
    free(ce[i]);
  }
}

int generate_prach_sequences(){
  for(int i=0;i<NOF_PRACH_SEQUENCES;i++){
    if(prach_gen(&prach, i, 2, prach_buffers[i])){
      fprintf(stderr, "Error generating prach sequence\n");
      return -1;
    }
  }
  return 0;
}


int mib_decoder_init(int cell_id) {

  if (chest_ref_LTEDL(&chest, cell_id)) {
    fprintf(stderr, "Error initializing reference signal\n");
    return -1;
  }

  if (pbch_init(&pbch, 6, cell_id, CPNORM)) {
    fprintf(stderr, "Error initiating PBCH\n");
    return -1;
  }
  DEBUG("PBCH initiated cell_id=%d\n", cell_id);
  return 0;
}

int mib_decoder_run(cf_t *input, pbch_mib_t *mib) {
  int i, n;
  lte_fft_run(&fft, input, fft_buffer);

  /* Get channel estimates for each port */
  for (i=0;i<NOF_PORTS;i++) {
    chest_ce_slot_port(&chest, fft_buffer, ce[i], 1, i);
  }

  DEBUG("Decoding PBCH\n", 0);
  n = pbch_decode(&pbch, fft_buffer, ce, 1, mib);

  return n;
}

void sigintHandler(int sig_num)
{
  go_exit=1;
}

int main(int argc, char **argv) {
  int frame_cnt;
  int cell_id;
  int find_idx, track_idx, last_found;
  enum sync_state state;
  int nslot;
  pbch_mib_t mib;
  float cfo;
  int n;
  int nof_found_mib = 0;
  float timeoffset = 0;
  timestamp_t uhd_time;
  timestamp_t pss_time;
  timestamp_t next_frame_time;
  timestamp_t next_prach_time;

  verbose = VERBOSE_NONE;
  parse_args(argc,argv);

  if (base_init(FLEN)) {
    fprintf(stderr, "Error initializing memory\n");
    exit(-1);
  }

  sync_pss_det_peak_to_avg(&sfind);
  sync_pss_det_peak_to_avg(&strack);

  if (!input_file_name) {
    double f, r, g;
    r = cuhd_set_rx_srate(uhd, SAMP_FREQ);
    g = cuhd_set_rx_gain(uhd, uhd_rx_gain);
    f = cuhd_set_rx_freq(uhd, uhd_rx_freq);
    cuhd_rx_wait_lo_locked(uhd);
    INFO("Set uhd rx frequency %.3f MHz, rate %.3f MHz, gain %.3f dB\n",
          f/1000000,
          r/1000000,
          g);

    r = cuhd_set_tx_srate(uhd, uhd_tx_rate);
    g = cuhd_set_tx_gain(uhd, uhd_tx_gain);
    f = cuhd_set_tx_freq_offset(uhd, uhd_tx_freq, uhd_tx_offset);
    INFO("Set uhd tx frequency %.3f MHz, rate %.3f MHz, gain %.3f dB\n",
          f/1000000,
          r/1000000,
          g);

    DEBUG("Starting receiver...\n",0);
    cuhd_start_rx_stream(uhd);
  }

  generate_prach_sequences();

  printf("\n --- Press Ctrl+C to exit --- \n");
  signal(SIGINT, sigintHandler);

  state = FIND;
  nslot = 0;
  find_idx = 0;
  cfo = 0;
  mib.sfn = -1;
  frame_cnt = 0;
  last_found = 0;
  sync_set_threshold(&sfind, find_threshold);
  sync_force_N_id_2(&sfind, -1);

  while(!go_exit && (frame_cnt < nof_frames || nof_frames==-1)) {
    INFO(" -----   RECEIVING %d SAMPLES ---- \n", FLEN);
    if (input_file_name) {
      n = filesource_read(&fsrc, input_buffer, FLEN);
      if (n == -1) {
        fprintf(stderr, "Error reading file\n");
        exit(-1);
      } else if (n < FLEN) {
        filesource_seek(&fsrc, 0);
        filesource_read(&fsrc, input_buffer, FLEN);
      }
    } else {
      cuhd_recv_timed(uhd, input_buffer, FLEN, 1, &uhd_time.full_secs, &uhd_time.frac_secs);
      INFO("UHD time = %.6f\n", uhd_time.full_secs+uhd_time.frac_secs);
    }

    switch(state) {
    case FIND:
      /* find peak in all frame */
      find_idx = sync_run(&sfind, input_buffer);
      timestamp_init(&pss_time, uhd_time.full_secs, uhd_time.frac_secs);
      timestamp_add(&pss_time, 0, find_idx/(double)SAMP_FREQ);
      INFO("FIND %3d:\tPAR=%.2f\tTIME=%.6f\n", frame_cnt, sync_get_peak_to_avg(&sfind), timestamp_real(&pss_time));
      if (find_idx != -1) {
        /* if found peak, go to track and set track threshold */
        cell_id = sync_get_cell_id(&sfind);
        if (cell_id != -1) {
          frame_cnt = -1;
          last_found = 0;
          sync_set_threshold(&strack, track_threshold);
          sync_force_N_id_2(&strack, sync_get_N_id_2(&sfind));
          sync_force_cp(&strack, sync_get_cp(&sfind));
          mib_decoder_init(cell_id);
          nof_found_mib = 0;
          nslot = sync_get_slot_id(&sfind);
          nslot=(nslot+10)%20;
          cfo = 0;
          timeoffset = 0;
          printf("\n");
          state = TRACK;
        } else {
          printf("cellid=-1\n");
        }
      }
      if (verbose == VERBOSE_NONE) {
        printf("Finding PSS... PAR=%.2f\r", sync_get_peak_to_avg(&sfind));
      }
      break;
    case TRACK:
      /* Find peak around known position find_idx */
      timestamp_init(&pss_time, uhd_time.full_secs, uhd_time.frac_secs);
      timestamp_add(&pss_time, 0, find_idx/(double)SAMP_FREQ);
      INFO("TRACK %3d: PSS find_idx %d offset %d PSS time %.6f\n", frame_cnt, find_idx, find_idx - track_len, timestamp_real(&pss_time));
      track_idx = sync_run(&strack, &input_buffer[find_idx - track_len]);

      if (track_idx != -1) {
        /* compute cumulative moving average CFO */
        cfo = (sync_get_cfo(&strack) + frame_cnt * cfo) / (frame_cnt + 1);
        /* compute cumulative moving average time offset */
        timeoffset = (float) (track_idx-track_len + timeoffset * frame_cnt) / (frame_cnt + 1);
        last_found = frame_cnt;
        find_idx = (find_idx + track_idx - track_len)%FLEN;
        if (nslot != sync_get_slot_id(&strack)) {
          INFO("Expected slot %d but got %d\n", nslot, sync_get_slot_id(&strack));
          printf("\r\n");
          fflush(stdout);
          printf("\r\n");
          state = FIND;
        }
      } else {
        /* if sync not found, adjust time offset with the averaged value */
        find_idx = (find_idx + (int) timeoffset)%FLEN;
      }

      /* if we missed too many PSS go back to FIND */
      if (frame_cnt - last_found > max_track_lost) {
        INFO("%d frames lost. Going back to FIND", frame_cnt - last_found);
        printf("\r\n");
        fflush(stdout);
        printf("\r\n");
        state = FIND;
      }

      // Correct CFO
      INFO("Correcting CFO=%.4f\n", cfo);

      cfo_correct(&cfocorr, input_buffer, -cfo/128);

      if (nslot == 0 && find_idx + 960 < FLEN) {
        timestamp_init(&next_frame_time, uhd_time.full_secs, uhd_time.frac_secs);
        timestamp_add(&next_frame_time, 0, find_idx/(double)SAMP_FREQ - pss_time_offset + 0.001);
        INFO("Next frame time = %.6f\n", timestamp_real(&next_frame_time));

        //Tx PRACH every 10 lte dl frames
        if(frame_cnt%20 < 2){
          printf("TX PRACH\n");
          timestamp_copy(&next_prach_time, &next_frame_time);
          timestamp_add(&next_prach_time, 0, prach_time_offset);
          //timestamp_add(&next_prach_time, 0, 0.01);
          cuhd_send_timed(uhd, prach_buffers[7], prach_buffer_len, 0,
                          next_prach_time.full_secs, next_prach_time.frac_secs);
        }

        INFO("Finding MIB at idx %d\n", find_idx);
        if (mib_decoder_run(&input_buffer[find_idx], &mib)) {
          INFO("MIB detected attempt=%d\n", frame_cnt);
          if (verbose == VERBOSE_NONE) {
            if (!nof_found_mib) {
              printf("\r\n");
              fflush(stdout);
              printf("\r\n");
              printf(" - Phy. CellId:\t%d\n", cell_id);
              pbch_mib_fprint(stdout, &mib);
            }
          }
          nof_found_mib++;
        } else {
          INFO("MIB not found attempt %d\n",frame_cnt);
        }
        if (frame_cnt) {
          printf("SFN: %4d, CFO: %+.4f KHz, SFO: %+.4f Khz, TimeOffset: %4d, Errors: %4d/%4d, ErrorRate: %.1e\r", mib.sfn,
              cfo*15, timeoffset/5, find_idx, frame_cnt-2*(nof_found_mib-1), frame_cnt,
              (float) (frame_cnt-2*(nof_found_mib-1))/frame_cnt);
          fflush(stdout);
        }
      }
      if (input_file_name) {
        usleep(5000);
      }
      nslot = (nslot+10)%20;
      break;
    }
    frame_cnt++;
  }

  base_free();

  printf("\nBye\n");
  exit(0);
}

