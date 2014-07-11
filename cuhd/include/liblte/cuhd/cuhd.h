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



#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

#include "liblte/config.h"
#include "liblte/cuhd/cuhd_utils.h"

/*!
 * A basic C-wrapper for the UHD multi_usrp device class:
 *
 * This wrapper provides a transparent handler for the multi_usrp
 * class and supports tx and rx streams with timestamps.
 */

/*!
 * Create a USRP connection and intialize the handle.
 * \param args arguments used to find and initialize USRP.
 * \param handler generic pointer to USRP handler.
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_open(char *args, void **handler);

/*!
 * Close the USRP connection and destroy the handle.
 * \param h generic pointer to USRP handler.
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_close(void *h);

/*!
 * Set master clock rate and source
 * \param h generic pointer to USRP handler.
 * \param rate desired master clock rate
 * \param source master clock source ("internal", "external", "MIMO")
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_set_master_clock(void *h, double rate, char* source);

/*!
 * Start rx stream
 * \param h generic pointer to USRP handler.
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_start_rx_stream(void *h);

/*!
 * Stream a set number of samples
 * \param h generic pointer to USRP handler.
 * \param nsamples number of samples to stream.
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_start_rx_stream_nsamples(void *h, int nsamples);

/*!
 * Stop rx stream
 * \param h generic pointer to USRP handler.
 * \return error code - success = 0.
 */
LIBLTE_API int cuhd_stop_rx_stream(void *h);

/*!
 * Wait for LO lock on device
 * \param h generic pointer to USRP handler.
 * \return true if locked.
 */
LIBLTE_API bool cuhd_rx_wait_lo_locked(void *h);

/*!
 * Set rx sampling rate
 * \param h generic pointer to USRP handler.
 * \param rate desired sampling rate
 * \return the rate in Sps.
 */
LIBLTE_API double cuhd_set_rx_srate(void *h, double rate);

/*!
 * Set rx gain
 * \param h generic pointer to USRP handler.
 * \param gain desired gain
 * \return the gain in dB.
 */
LIBLTE_API double cuhd_set_rx_gain(void *h, double gain);

/*!
 * Set rx frequency
 * \param h generic pointer to USRP handler.
 * \param freq desired frequency
 * \return the frequency in Hz.
 */
LIBLTE_API double cuhd_set_rx_freq(void *h, double freq);

/*!
 * Set rx frequency and specify desired LO offset
 * \param h generic pointer to USRP handler.
 * \param freq desired frequency
 * \param off desired LO offset
 * \return the frequency in Hz.
 */
LIBLTE_API double cuhd_set_rx_freq_offset(void *h, double freq, double off);

/*!
 * Receive samples
 * \param h generic pointer to USRP handler.
 * \param data buffer to hold incoming data
 * \param nsamples number of samples to receive
 * \param blocking wait until nsamples are received if true
 * \return the number of samples received
 */
LIBLTE_API int cuhd_recv(void *h, void *data, int nsamples, int blocking);

/*!
 * Receive samples with timestamp
 * \param h generic pointer to USRP handler.
 * \param data buffer to hold incoming data
 * \param nsamples number of samples to receive
 * \param blocking wait until nsamples are received if true
 * \param secs will be set with number of full secs in timestamp
 * \param frac_secs will be set with number of fractional secs in timestamp
 * \return the number of samples received
 */
LIBLTE_API int cuhd_recv_timed(void *h,
                               void *data,
                               int nsamples,
                               int blocking,
                               time_t *secs,
                               double *frac_secs);

/*!
 * Set tx sampling rate
 * \param h generic pointer to USRP handler.
 * \param rate desired sampling rate
 * \return the rate in Sps.
 */
LIBLTE_API double cuhd_set_tx_srate(void *h, double rate);

/*!
 * Set tx gain
 * \param h generic pointer to USRP handler.
 * \param rate desired gain
 * \return the gain in dB
 */
LIBLTE_API double cuhd_set_tx_gain(void *h, double gain);

/*!
 * Set tx frequency
 * \param h generic pointer to USRP handler.
 * \param rate desired frequency
 * \return the frequency in Hz.
 */
LIBLTE_API double cuhd_set_tx_freq(void *h, double freq);

/*!
 * Set tx frequency and specify desired LO offset
 * \param h generic pointer to USRP handler.
 * \param rate desired frequency
 * \param off desired LO offset
 * \return the frequency in Hz.
 */
LIBLTE_API double cuhd_set_tx_freq_offset(void *h, double freq, double off);

/*!
 * Send samples
 * \param h generic pointer to USRP handler.
 * \param data buffer holding samples
 * \param nsamples number of samples in buffer
 * \param blocking wait until nsamples are sent if true
 * \return the number of samples sent
 */
LIBLTE_API int cuhd_send(void *h, void *data, int nsamples, int blocking);

/*!
 * Send samples at a specific time
 * \param h generic pointer to USRP handler.
 * \param data buffer holding samples
 * \param nsamples number of samples in buffer
 * \param blocking wait until nsamples are sent if true
 * \param secs full secs of timestamp
 * \param frac_secs fractional secs of timestamp
 * \return the number of samples sent
 */
LIBLTE_API int cuhd_send_timed(void *h,
                               void *data,
                               int nsamples,
                               int blocking,
                               time_t secs,
                               double frac_secs);


#ifdef __cplusplus
}
#endif
