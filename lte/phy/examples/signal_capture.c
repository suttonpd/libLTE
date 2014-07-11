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
#define FLEN          9600
#define FLEN_PERIOD   0.005

char *output_file_name = "capture.bin";
int go_exit=0;

float uhd_freq = 2600000000.0, uhd_gain = 20.0, uhd_rate = 1920000;
char *uhd_args = "";

filesink_t fsink;
cf_t *input_buffer;

void usage(char *prog) {
  printf("Usage: %s [oagfrv]\n", prog);
  printf("\t-o output_file [Default capture.bin]\n");
  printf("\t-a UHD args [Default %s]\n", uhd_args);
  printf("\t-g UHD RX gain [Default %.2f dB]\n", uhd_gain);
  printf("\t-f UHD RX frequency [Default %.1f MHz]\n", uhd_freq/1000000);
  printf("\t-r UHD RX rate [Default %.3f MHz]\n", uhd_rate/1000000);
  printf("\t-v [set verbose to debug, default none]\n");
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "oagfrv")) != -1) {
    switch(opt) {
    case 'o':
      output_file_name = argv[optind];
      break;
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
    case 'v':
      verbose++;
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
}

int base_init(int frame_length) {
  if (filesink_init(&fsink, output_file_name, COMPLEX_FLOAT_BIN)) {
    return -1;
  }

  /* open UHD device */
  printf("Opening UHD device...\n");
  if (cuhd_open(uhd_args,&uhd)) {
    fprintf(stderr, "Error opening uhd\n");
    return -1;
  }

  input_buffer = (cf_t*) malloc(FLEN * sizeof(cf_t));
  if (!input_buffer) {
    perror("malloc");
    return -1;
  }

  return 0;
}

void base_free() {
  filesink_free(&fsink);
  cuhd_close(uhd);
  free(input_buffer);
}

void sigintHandler(int sig_num)
{
  go_exit=1;
}

int main(int argc, char **argv) {

  parse_args(argc,argv);

  if (base_init(FLEN)) {
    fprintf(stderr, "Error initializing memory\n");
    exit(-1);
  }

  INFO("Setting sampling rate %.2f MHz\n", (float) uhd_rate/MHZ);
  cuhd_set_rx_srate(uhd, uhd_rate);
  cuhd_set_rx_gain(uhd, uhd_gain);
  cuhd_set_rx_freq(uhd, (double) uhd_freq);
  cuhd_rx_wait_lo_locked(uhd);
  DEBUG("Set uhd_freq to %.3f MHz\n", (double) uhd_freq);

  DEBUG("Starting receiver...\n",0);
  cuhd_start_rx_stream(uhd);


  printf("\n --- Press Ctrl+C to exit --- \n");
  signal(SIGINT, sigintHandler);

  while(!go_exit) {
    INFO(" -----   RECEIVING %d SAMPLES ---- \n", FLEN);
    cuhd_recv(uhd, input_buffer, FLEN, 1);
    filesink_write(&fsink, input_buffer, FLEN);
  }

  base_free();

  printf("\nBye\n");
  exit(0);
}

