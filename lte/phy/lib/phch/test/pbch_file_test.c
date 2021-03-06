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

#include "liblte/phy/phy.h"

char *input_file_name = NULL;
char *matlab_file_name = NULL;

FILE *fmatlab = NULL;

lte_cell_t cell = {
  6,            // nof_prb
  2,            // nof_ports
  150,          // cell_id
  CPNORM        // cyclic prefix
};

#define FLEN  9600

filesource_t fsrc;
cf_t *input_buffer, *fft_buffer, *ce[MAX_PORTS];
pbch_t pbch;
lte_fft_t fft;
chest_t chest;

void usage(char *prog) {
  printf("Usage: %s [vcoe] -i input_file\n", prog);
  printf("\t-o output matlab file name [Default Disabled]\n");
  printf("\t-c cell_id [Default %d]\n", cell.id);
  printf("\t-n nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-e Set extended prefix [Default Normal]\n");
  printf("\t-v [set verbose to debug, default none]\n");
}

void parse_args(int argc, char **argv) {
  int opt;

  while ((opt = getopt(argc, argv, "iovce")) != -1) {
    switch(opt) {
    case 'i':
      input_file_name = argv[optind];
      break;
    case 'c':
      cell.id = atoi(argv[optind]);
      break;
    case 'o':
      matlab_file_name = argv[optind];
      break;
    case 'v':
      verbose++;
      break;
    case 'e':
      cell.cp = CPEXT;
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
  if (!input_file_name) {
    usage(argv[0]);
    exit(-1);
  }
}

int base_init() {
  int i;

  if (filesource_init(&fsrc, input_file_name, COMPLEX_FLOAT_BIN)) {
    fprintf(stderr, "Error opening file %s\n", input_file_name);
    exit(-1);
  }

  if (matlab_file_name) {
    fmatlab = fopen(matlab_file_name, "w");
    if (!fmatlab) {
      perror("fopen");
      return -1;
    }
  } else {
    fmatlab = NULL;
  }

  input_buffer = malloc(FLEN * sizeof(cf_t));
  if (!input_buffer) {
    perror("malloc");
    exit(-1);
  }

  fft_buffer = malloc(SLOT_LEN_RE(cell.nof_prb, cell.cp) * sizeof(cf_t));
  if (!fft_buffer) {
    perror("malloc");
    return -1;
  }

  for (i=0;i<cell.nof_ports;i++) {
    ce[i] = malloc(SLOT_LEN_RE(cell.nof_prb, cell.cp) * sizeof(cf_t));
    if (!ce[i]) {
      perror("malloc");
      return -1;
    }
  }
  
  if (!lte_cell_isvalid(&cell)) {
    fprintf(stderr, "Invalid cell properties\n");
    return -1;
  }

  if (chest_init_LTEDL(&chest, cell)) {
    fprintf(stderr, "Error initializing equalizer\n");
    return -1;
  }

  if (lte_fft_init(&fft, cell.cp, cell.nof_prb)) {
    fprintf(stderr, "Error initializing FFT\n");
    return -1;
  }

  if (pbch_init(&pbch, cell)) {
    fprintf(stderr, "Error initiating PBCH\n");
    return -1;
  }

  DEBUG("Memory init OK\n",0);
  return 0;
}

void base_free() {
  int i;

  filesource_free(&fsrc);
  if (fmatlab) {
    fclose(fmatlab);
  }

  free(input_buffer);
  free(fft_buffer);

  filesource_free(&fsrc);
  for (i=0;i<cell.nof_ports;i++) {
    free(ce[i]);
  }
  chest_free(&chest);
  lte_fft_free(&fft);

  pbch_free(&pbch);
}

int main(int argc, char **argv) {
  pbch_mib_t mib;
  int n;

  if (argc < 3) {
    usage(argv[0]);
    exit(-1);
  }

  parse_args(argc,argv);

  if (base_init()) {
    fprintf(stderr, "Error initializing receiver\n");
    exit(-1);
  }

  n = filesource_read(&fsrc, input_buffer, FLEN);

  lte_fft_run_slot(&fft, &input_buffer[960], fft_buffer);

  if (fmatlab) {
    fprintf(fmatlab, "outfft=");
    vec_sc_prod_cfc(fft_buffer, 1000.0, fft_buffer, CP_NSYMB(cell.cp) * cell.nof_prb * RE_X_RB);
    vec_fprint_c(fmatlab, fft_buffer, CP_NSYMB(cell.cp) * cell.nof_prb * RE_X_RB);
    fprintf(fmatlab, ";\n");
    vec_sc_prod_cfc(fft_buffer, 0.001, fft_buffer,   CP_NSYMB(cell.cp) * cell.nof_prb * RE_X_RB);
  }

  /* Get channel estimates for each port */
  chest_ce_slot(&chest, fft_buffer, ce, 1);

  INFO("Decoding PBCH\n", 0);

  n = pbch_decode(&pbch, fft_buffer, ce, &mib);

  base_free();

  if (n < 0) {
    fprintf(stderr, "Error decoding PBCH\n");
    exit(-1);
  } else if (n == 0) {
    printf("Could not decode PBCH\n");
    exit(-1);
  } else {
    if (mib.nof_ports == 2 && mib.nof_prb == 50 && mib.phich_length == PHICH_NORM
        && mib.phich_resources == R_1 && mib.sfn == 28) {
      pbch_mib_fprint(stdout, &mib, cell.id);
      printf("This is the signal.1.92M.dat file\n");
      exit(0);
    } else {
      pbch_mib_fprint(stdout, &mib, cell.id);
      printf("This is an unknown file\n");
      exit(-1);
    }
  }
}
