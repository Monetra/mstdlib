/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __M_IO_BWSHAPING_H__
#define __M_IO_BWSHAPING_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_bwshaping Addon for bandwidth shaping
 *  \ingroup m_eventio_base_addon
 * 
 * Bandwidth Shaping Addon
 *
 * Allows tracking and altering data flow though an io object.
 *
 * Common uses:
 * - Tracking bandwidth utilization.
 * - Rate limiting download speed for connected clients.
 * - Throttling to prevent network link saturation.
 * - Testing real world network setups.
 * - Determining how an application will preform in a bad environment.
 *
 * @{
 */

/*! Method of shaping. */
enum M_io_bwshaping_mode {
	M_IO_BWSHAPING_MODE_BURST   = 1, /*!< Allow bursting of data for each throttle period */
	M_IO_BWSHAPING_MODE_TRICKLE = 2  /*!< Enforce data to be more evenly distributed across the throttle period */
};
typedef enum M_io_bwshaping_mode M_io_bwshaping_mode_t;


/*! Shaping direction. */
enum M_io_bwshaping_direction {
	M_IO_BWSHAPING_DIRECTION_IN  = 1,
	M_IO_BWSHAPING_DIRECTION_OUT = 2
};
typedef enum M_io_bwshaping_direction M_io_bwshaping_direction_t;

/*! Add a BWShaping layer. 
 *
 * Adding a layer without any settings will track bandwidth utilization
 *
 * \param[in]  io       io object.
 * \param[out] layer_id Layer id this is added at.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_add_bwshaping(M_io_t *io, size_t *layer_id);

/*! Throttle data by bytes per seconds.
 *
 * \param[in] io        io object.
 * \param[in] layer_id  Layer id to apply this to. This should be the same layer id returned form M_io_add_bwshaping.
 * \param[in] direction Direction this applies to.
 * \param[in] Bps       Bytes per seconds that should be throttled. Bps = 0 is infinite.
 *
 * \return Result.
 */
M_API M_bool M_io_bwshaping_set_throttle(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction, M_uint64 Bps);


/*! Set how the data throttling should function.
 *
 * \param[in] io        io object.
 * \param[in] layer_id  Layer id to apply this to. This should be the same layer id returned form M_io_add_bwshaping.
 * \param[in] direction Direction this applies to.
 * \param[in] mode      Throttle mode.
 *
 * \return Result.
 */
M_API M_bool M_io_bwshaping_set_throttle_mode(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction, M_io_bwshaping_mode_t mode);


/*! Set throttle period.
 *
 * The period must be longer than the sample frequency. Meaning period * 1000 > sample frequency.
 *
 * \param[in] io                  io object.
 * \param[in] layer_id            Layer id to apply this to. This should be the same layer id returned
 *                                form M_io_add_bwshaping.
 * \param[in] direction           Direction this applies to.
 * \param[in] period_s            Period in seconds. Must be at least 1.
 * \param[in] sample_frequency_ms Frequency in milliseconds. Must be at least 15.
 *
 * \return Result.
 */
M_API M_bool M_io_bwshaping_set_throttle_period(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction, M_uint64 period_s, M_uint64 sample_frequency_ms);


/*! Set latency.
 *
 * \param[in] io         io object.
 * \param[in] layer_id   Layer id to apply this to. This should be the same layer id returned
 *                       form M_io_add_bwshaping.
 * \param[in] direction  Direction this applies to.
 * \param[in] latency_ms Latency in milliseconds. 0 is no latency.
 *
 * \return Result.
 */
M_API M_bool M_io_bwshaping_set_latency(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction, M_uint64 latency_ms);


/*! Get current Bps in period for direction
 *
 * \param[in] io        io object.
 * \param[in] layer_id  Layer id to apply this to. This should be the same layer id returned
 *                      form M_io_add_bwshaping.
 * \param[in] direction Direction this applies to.
 *
 * \return Bps.
 */
M_API M_uint64 M_io_bwshaping_get_Bps(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction);



/*! Get total number of bytes transferred for direction since beginning
 *
 * \param[in] io        io object.
 * \param[in] layer_id  Layer id to apply this to. This should be the same layer id returned
 *                      form M_io_add_bwshaping.
 * \param[in] direction Direction this applies to.
 *
 * \return Count of bytes.
 */
M_API M_uint64 M_io_bwshaping_get_totalbytes(M_io_t *io, size_t layer_id, M_io_bwshaping_direction_t direction);

/*! Get total time of connection in ms
 *
 * \param[in] io        io object.
 * \param[in] layer_id  Layer id to apply this to. This should be the same layer id returned
 *                      form M_io_add_bwshaping.
 *
 * \return Milliseconds.
 */
M_API M_uint64 M_io_bwshaping_get_totalms(M_io_t *io, size_t layer_id);

/*! @} */

__END_DECLS
#endif
