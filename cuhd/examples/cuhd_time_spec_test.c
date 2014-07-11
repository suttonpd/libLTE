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

#include <stdint.h>
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
#include <complex.h>

#include "liblte/cuhd/cuhd.h"

typedef _Complex float cf_t;

void *uhd;
float uhd_freq = 2600000000.0, uhd_gain = 20.0, uhd_rate = 1920000.0;
char *uhd_args = "";

int go_exit=0;

cf_t *input_buffer;

void usage(char *prog) {
  printf("Usage: %s [agfr]\n", prog);
  printf("\t-a UHD args [Default %s]\n", uhd_args);
  printf("\t-g UHD RX gain [Default %.2f dB]\n", uhd_gain);
  printf("\t-f UHD RX frequency [Default %.3f MHz]\n", uhd_freq/1000000);
  printf("\t-r UHD RX rate [Default %.3f MHz]\n", uhd_rate/1000000);
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "agfr")) != -1) {
    switch(opt) {
    case 'a':
      uhd_args = argv[optind];
      break;
    case 'g':
      uhd_gain = atof(argv[optind]);
      break;
    case 'f':
      uhd_freq = atof(argv[optind]);
      break;
    case 'r':
      uhd_rate = atof(argv[optind]);
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
}

int base_init() {
  printf("Opening UHD device...\n");
  if (cuhd_open(uhd_args,&uhd)) {
    fprintf(stderr, "Error opening uhd\n");
    return -1;
  }

  //Allocate a buffer of 10M samples
  input_buffer = (cf_t*) malloc(10000000 * sizeof(cf_t));
  if (!input_buffer) {
    perror("malloc");
    return -1;
  }

  return 0;
}

void base_free() {
  cuhd_close(uhd);
  free(input_buffer);
}


void sigintHandler(int sig_num)
{
  go_exit=1;
}

int main(int argc, char **argv) {

  parse_args(argc,argv);

  if (base_init()) {
    fprintf(stderr, "Error initializing memory\n");
    exit(-1);
  }

  printf("Setting sampling rate %.3f MHz\n", (float) uhd_rate/1000000.0);
  cuhd_set_rx_srate(uhd, uhd_rate);
  cuhd_set_rx_gain(uhd, uhd_gain);

  printf("Setting frequency to %.3f MHz\n", (double) uhd_freq);
  cuhd_set_rx_freq(uhd, (double) uhd_freq);
  cuhd_rx_wait_lo_locked(uhd);

  printf("Starting receiver...\n",0);
  cuhd_start_rx_stream(uhd);

  printf("\n --- Press Ctrl+C to exit --- \n");
  signal(SIGINT, sigintHandler);

  time_t secs;
  double frac_secs;
  uint32_t count = 0;
  while(!go_exit) {
    count++;
    printf(" -----   RECEIVING %f SAMPLES ---- \n", uhd_rate);
    cuhd_recv_timed(uhd, input_buffer, uhd_rate, 1, &secs, &frac_secs);
    printf("Got uhd frame with secs %i, frac_secs %f\n", secs, frac_secs);

    if(count>5 && uhd_rate < 5000000){
      uhd_rate = uhd_rate*2;
      printf("Setting sampling frequency %.3f MHz\n", (float) uhd_rate/1000000.0);
      cuhd_set_rx_srate(uhd, uhd_rate);
      count = 0;
    }
  }

  base_free();

  printf("\nBye\n");
  exit(0);
}

