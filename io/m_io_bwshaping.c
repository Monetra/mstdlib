/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"

/* BWShaping does bandwidth shaping, whether to impose read/write bandwidth
 * restrictions, or to inject latency into a connection.  It also tracks
 * bandwidth for the connection, even if shaping is not in use.
 * 
 * Shaping occurs by calculating the number of bytes allowed to be written
 * or read to stay within the constraints, and if that number is zero, it
 * calculates the time, in milliseconds when the next read or write is allowed
 * to take place.  This offset is put into a timer which will then signal
 * a read or write event as appropriate.
 * 
 * If shaping is not in use, no timers will be used.
 * 
 * Timers get activated on these conditions:
 *  * A write is attempted, and the allowed size is either zero, or less
 *    than requested, set flag out_waiting = M_TRUE, otherwise set
 *    out_waiting = M_FALSE.  If the size is non-zero, and the OS-write
 *    operation returns WOULDBLOCK *or* returns less than the requested
 *    number of bytes, then the timer should *NOT* be activated as we can
 *    wait on an OS event (out_waiting = M_FALSE).
 *    Set timers based on out_waiting and calculated metrics.
 *  * A read is attempted, and the allowed size is either zero, or less
 *    than requested, set flag in_waiting = M_TRUE, otherwise set 
 *    in_waiting = M_FALSE.  If the size is non-zero, and the OS-read operation
 *    returns WOULDBLOCK *or* returns less than the requested number of bytes,
 *    then the timer should *NOT* be activated as we can wait on an OS event
 *    (in_waiting = M_FALSE).
 *    Set timers based on in_waiting and calculated metrics.
 *  * A write EVENT is received, and the allowed size is zero, set
 *    out_waiting = M_TRUE, otherwise set out_waiting = M_FALSE.  If out_waiting
 *    state changed, then Set timers based on out_waiting and calculated
 *    metrics.
 *  * A read EVENT is received, and the allowed size is zero, set
 *    in_waiting = M_TRUE, otherwise set in_waiting = M_FALSE.  If in_waiting
 *    state changed, then Set timers based on in_waiting and calculated
 *    metrics.
 */

#define M_IO_BWSHAPING_NAME "BWSHAPING"

struct M_io_bwshaping_slot {
	M_uint64    bytes;
	M_timeval_t tv;
};
typedef struct M_io_bwshaping_slot M_io_bwshaping_slot_t;

struct M_io_bwshaping_bwtrack {
	M_uint64               bytes;
	M_io_bwshaping_slot_t *slots;
	size_t                 slots_size;
	size_t                 slots_len;
	size_t                 slots_start;
};
typedef struct M_io_bwshaping_bwtrack M_io_bwshaping_bwtrack_t;


struct M_io_bwshaping_settings {
	/* In */
	M_uint64              in_Bps;
	M_io_bwshaping_mode_t in_mode;
	M_uint64              in_period_s;
	M_uint64              in_sample_frequency_ms;
	M_uint64              in_latency_ms;

	/* Out */
	M_uint64              out_Bps;
	M_io_bwshaping_mode_t out_mode;
	M_uint64              out_period_s;
	M_uint64              out_sample_frequency_ms;
	M_uint64              out_latency_ms;
};
typedef struct M_io_bwshaping_settings M_io_bwshaping_settings_t;

struct M_io_handle {
	M_io_t                   *io;
	/* Settings */
	M_io_bwshaping_settings_t settings;

	M_timeval_t               starttv;

	M_io_bwshaping_bwtrack_t *in_bw;
	M_timeval_t               in_lasttv;
	M_uint64                  in_bytes;
	/* Record if last read operation was throttled and therefore needs to set a timer */
	M_bool                    in_waiting;
	/* Record if last read attempt returned as many bytes as requested by the caller. If this
	 * is true, then we assume there will be another read attempt and bypass latency checks */
	M_bool                    in_fullread;

	M_io_bwshaping_bwtrack_t *out_bw;
	M_timeval_t               out_lasttv;
	M_uint64                  out_bytes;
	/* Record if last write operation was throttled and therefore needs to set a timer */
	M_bool                    out_waiting;

	M_event_timer_t          *timer;
};

static M_io_bwshaping_slot_t *M_io_bwshaping_slots_at(M_io_bwshaping_bwtrack_t *bwtrack, size_t idx)
{
	if (idx >= bwtrack->slots_len)
		return NULL;
	return &bwtrack->slots[(bwtrack->slots_start + idx) % bwtrack->slots_size];
}

static M_io_bwshaping_slot_t *M_io_bwshaping_slots_first(M_io_bwshaping_bwtrack_t *bwtrack)
{
	return M_io_bwshaping_slots_at(bwtrack, 0);
}

static M_io_bwshaping_slot_t *M_io_bwshaping_slots_last(M_io_bwshaping_bwtrack_t *bwtrack)
{
	return M_io_bwshaping_slots_at(bwtrack, bwtrack->slots_len-1);
}

static M_io_bwshaping_slot_t *M_io_bwshaping_slots_add(M_io_bwshaping_bwtrack_t *bwtrack)
{
	if (bwtrack->slots_len == bwtrack->slots_size)
		return NULL;
	bwtrack->slots_len++;
	return M_io_bwshaping_slots_last(bwtrack);
}

static M_bool M_io_bwshaping_slots_remove_first(M_io_bwshaping_bwtrack_t *bwtrack)
{
	M_io_bwshaping_slot_t *slot = M_io_bwshaping_slots_first(bwtrack);
	if (slot == NULL)
		return M_FALSE;

	/* As long as the consumer doesn't assume the data is zero, it should
	 * be safe to not memset this:
	 *	M_mem_set(slot, 0, sizeof(*slot)); */
	bwtrack->slots_len--;
	bwtrack->slots_start++;
	if (bwtrack->slots_start == bwtrack->slots_size)
		bwtrack->slots_start = 0;

	return M_TRUE;
}

static M_bool M_io_bwshaping_slots_create(M_io_bwshaping_bwtrack_t *bwtrack, size_t max_slots)
{
	bwtrack->slots_size  = max_slots;
	bwtrack->slots_start = 0;
	bwtrack->slots_len   = 0;
	bwtrack->slots       = M_malloc_zero(sizeof(*bwtrack->slots) * bwtrack->slots_size);
	return M_TRUE;
}

static M_bool M_io_bwshaping_slots_destroy(M_io_bwshaping_bwtrack_t *bwtrack)
{
	bwtrack->slots_size  = 0;
	bwtrack->slots_start = 0;
	bwtrack->slots_len   = 0;
	M_free(bwtrack->slots);
	return M_TRUE;
}

static void M_io_bwshaping_bwtrack_add(M_io_bwshaping_bwtrack_t *bwtrack, M_uint64 bytes, M_uint64 frequency_ms)
{
	M_io_bwshaping_slot_t *slot;

	slot = M_io_bwshaping_slots_last(bwtrack);
	if (slot == NULL || M_time_elapsed(&slot->tv) > frequency_ms) {
		slot = M_io_bwshaping_slots_add(bwtrack);
		M_time_elapsed_start(&slot->tv);
		slot->bytes = 0;
	}
	slot->bytes    += bytes;
	bwtrack->bytes += bytes;
}


static void M_io_bwshaping_bwtrack_remove_expired(M_io_bwshaping_bwtrack_t *bwtrack, M_uint64 keep_ms)
{
	M_io_bwshaping_slot_t *slot = NULL;

	while ((slot = M_io_bwshaping_slots_first(bwtrack)) != NULL) {
		if (M_time_elapsed(&slot->tv) < keep_ms)
			break;

		/* Delete stale entry */
		bwtrack->bytes -= slot->bytes;
		M_io_bwshaping_slots_remove_first(bwtrack);
	}
}


static M_uint64 M_io_bwshaping_bwtrack_bytesinperiod(M_io_bwshaping_bwtrack_t *bwtrack, M_uint64 frequency_ms)
{
	M_io_bwshaping_slot_t *slot;
	M_uint64               elapsed;

	slot = M_io_bwshaping_slots_last(bwtrack);
	if (slot == NULL)
		return 0;
	
	elapsed = M_time_elapsed(&slot->tv);
	if (elapsed >= frequency_ms)
		return 0;

	return slot->bytes;
}


static M_uint64 M_io_bwshaping_bwtrack_max_size(M_io_bwshaping_bwtrack_t *bwtrack, M_uint64 Bps, M_uint64 period_s, M_uint64 frequency_ms, M_io_bwshaping_mode_t mode)
{
	M_uint64 max_bytes_per_period;
	M_uint64 slots_per_period;

	M_io_bwshaping_bwtrack_remove_expired(bwtrack, period_s * 1000);

	if (Bps == 0)
		return M_UINT64_MAX;

	if (mode == M_IO_BWSHAPING_MODE_BURST) {
		max_bytes_per_period = Bps * period_s;
		if (max_bytes_per_period <= bwtrack->bytes)
			return 0;

		return max_bytes_per_period - bwtrack->bytes;
	}

	/* mode == TRICKLE */
	slots_per_period     = (period_s * 1000) / frequency_ms;
	max_bytes_per_period = (Bps * period_s) / slots_per_period;
	if (max_bytes_per_period == 0)
		max_bytes_per_period = 1;

	return max_bytes_per_period - M_io_bwshaping_bwtrack_bytesinperiod(bwtrack, frequency_ms);
}


static M_uint64 M_io_bwshaping_bwtrack_next_ms(M_io_bwshaping_bwtrack_t *bwtrack, M_uint64 Bps, M_uint64 period_s, M_uint64 frequency_ms, M_io_bwshaping_mode_t mode)
{
	M_io_bwshaping_slot_t *slot;
	M_uint64               elapsed_ms;
	M_uint64               next_ms;
	M_uint64               max_size;

	/* If we could send immediately, next_ms is definitely 0, no need to delay! */
	max_size = M_io_bwshaping_bwtrack_max_size(bwtrack, Bps, period_s, frequency_ms, mode);
	if (max_size != 0)
		return 0;

	if (mode == M_IO_BWSHAPING_MODE_BURST) {
		/* Time should be the time the oldest (first) slot falls off */
		slot       = M_io_bwshaping_slots_first(bwtrack);
		elapsed_ms = M_time_elapsed(&slot->tv);
		next_ms    = (period_s * 1000) - elapsed_ms;
	} else {
		/* mode == TRICKLE */
		/* This is the time between the frequency_ms and how much time has passed since the last
		 * slot */
		slot       = M_io_bwshaping_slots_last(bwtrack);
		elapsed_ms = M_time_elapsed(&slot->tv);
		next_ms    = frequency_ms - elapsed_ms;
	}

	return next_ms;
}


static size_t M_io_bwshaping_bwtrack_size(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	if (direction == M_IO_BWSHAPING_DIRECTION_IN)
		return (size_t)((handle->settings.in_period_s * 1000) / handle->settings.in_sample_frequency_ms) + 1;
	return (size_t)((handle->settings.out_period_s * 1000) / handle->settings.out_sample_frequency_ms) + 1;
}


static M_io_bwshaping_bwtrack_t *M_io_bwshaping_bwtrack_create(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_io_bwshaping_bwtrack_t *bwtrack = M_malloc_zero(sizeof(*bwtrack));

	M_io_bwshaping_slots_create(bwtrack, M_io_bwshaping_bwtrack_size(handle, direction)); 
	return bwtrack;
}


static void M_io_bwshaping_bwtrack_destroy(M_io_bwshaping_bwtrack_t *bwtrack)
{
	M_io_bwshaping_slots_destroy(bwtrack);
	M_free(bwtrack);
}


static void M_io_bwshaping_add_transfer(M_io_handle_t *handle, M_uint64 bytes, M_io_bwshaping_direction_t direction)
{
	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		handle->in_bytes += bytes;
		M_time_elapsed_start(&handle->in_lasttv);
		M_io_bwshaping_bwtrack_add(handle->in_bw, bytes, handle->settings.in_sample_frequency_ms);
		return;
	}
	handle->out_bytes += bytes;
	M_time_elapsed_start(&handle->out_lasttv);
	M_io_bwshaping_bwtrack_add(handle->out_bw, bytes, handle->settings.out_sample_frequency_ms);
}


static M_uint64 M_io_bwshaping_latency_next_ms(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_uint64 elapsed;

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		elapsed = M_time_elapsed(&handle->in_lasttv);
		if (handle->settings.in_latency_ms < elapsed) {
			return 0;
		} else if (handle->in_fullread) {
			/* Last read returned exactly as many bytes as requested, which means there is most likely
			 * more data to be read, so do not apply the latency to such an occurrence */
			return 0;
		}
		return handle->settings.in_latency_ms - elapsed;
	}

	/* M_IO_BWSHAPING_DIRECTION_OUT */
	elapsed = M_time_elapsed(&handle->out_lasttv);
	if (handle->settings.out_latency_ms < elapsed) {
		return 0;
	}
	return handle->settings.out_latency_ms - elapsed;
}

/* 
 *  - Returns M_TIMEOUT_INF if no timeout is being enforced at this moment.
 *  - Returns 0 if timeout has elapsed
 *  - Returns number of milliseconds remaining in timeout otherwise 
 */
static M_uint64 M_io_bwshaping_timeout_direction(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_uint64 bw_nextms;
	M_uint64 l_nextms;

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		/* Not configured, so don't use timeouts */
		if (handle->settings.in_Bps == 0 && handle->settings.in_latency_ms == 0)
			return M_TIMEOUT_INF;

		/* Don't use a timer if we're not throttling */
		if (!handle->in_waiting)
			return M_TIMEOUT_INF;

		bw_nextms = M_io_bwshaping_bwtrack_next_ms(handle->in_bw, handle->settings.in_Bps, handle->settings.in_period_s, handle->settings.in_sample_frequency_ms, handle->settings.in_mode);
	} else {
		/* Not configured, so don't use timeouts */
		if (handle->settings.out_Bps == 0 && handle->settings.out_latency_ms == 0)
			return M_TIMEOUT_INF;

		/* Don't use a timer if we're not throttling */
		if (!handle->out_waiting)
			return M_TIMEOUT_INF;

		bw_nextms = M_io_bwshaping_bwtrack_next_ms(handle->out_bw, handle->settings.out_Bps, handle->settings.out_period_s, handle->settings.out_sample_frequency_ms, handle->settings.out_mode);
	}

	l_nextms  = M_io_bwshaping_latency_next_ms(handle, direction);

	return M_MAX(bw_nextms, l_nextms);
}


static void M_io_bwshaping_clean_expired(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_io_bwshaping_bwtrack_remove_expired((direction == M_IO_BWSHAPING_DIRECTION_IN)?handle->in_bw:handle->out_bw,
		((direction == M_IO_BWSHAPING_DIRECTION_IN)?handle->settings.in_period_s:handle->settings.out_period_s)*1000);
}


static M_uint64 M_io_bwshaping_range_ms(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_io_bwshaping_bwtrack_t *bwtrack;
	M_io_bwshaping_slot_t    *slot;
	M_uint64                  range_ms;

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		bwtrack = handle->in_bw;
	} else {
		bwtrack = handle->out_bw;
	}

	slot = M_io_bwshaping_slots_first(bwtrack);
	if (slot == NULL)
		return 1;

	range_ms = M_time_elapsed(&slot->tv);
	if (range_ms <= 0)
		range_ms = 1;
	return range_ms;
}


static void M_io_bwshaping_set_timeout(M_io_handle_t *handle)
{
	M_uint64 in_ms;
	M_uint64 out_ms;
	M_uint64 to_ms;

	in_ms  = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_IN);
	out_ms = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_OUT);
	to_ms  = M_MIN(in_ms, out_ms);

	/* Don't use reset as a to_ms of 0 might be used */
	M_event_timer_stop(handle->timer);
	if (to_ms == M_TIMEOUT_INF)
		return;
	M_event_timer_start(handle->timer, to_ms);
}


static void M_io_bwshaping_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *io_bogus, void *arg)
{
	M_io_layer_t  *layer    = arg;
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	M_uint64       to_ms;
	size_t         num_wait = 0;
	(void)event;
	(void)type;
	(void)io_bogus;


	if (handle->in_waiting)
		num_wait++;
	if (handle->out_waiting)
		num_wait++;

	to_ms = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_IN);
	if (to_ms == 0) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
		num_wait--;
	}

	to_ms = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_OUT);
	if (to_ms == 0) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
		num_wait--;
	}

	/* If still waiting, re-queue timer */
	if (num_wait > 0) {
		M_io_bwshaping_set_timeout(handle);
	}
}


static M_bool M_io_bwshaping_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	/* Register timer */
	handle->timer = M_event_timer_add(M_io_get_event(io), M_io_bwshaping_timer_cb, layer);
	M_event_timer_set_firecount(handle->timer, 1);

	/* Start timer as appropriate */
	M_io_bwshaping_set_timeout(handle);

	return M_TRUE;
}


static M_io_error_t M_io_bwshaping_accept_cb(M_io_t *io, M_io_layer_t *orig_layer)
{
	M_io_error_t   err;
	size_t         layer_id;
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	M_io_handle_t *orig_handle;

	/* Add a new layer into the new comm object with the same settings as we have */
	err = M_io_add_bwshaping(io, &layer_id);
	if (err != M_IO_ERROR_SUCCESS)
		return err;

	layer = M_io_layer_acquire(io, layer_id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return M_IO_ERROR_ERROR;

	handle      = M_io_layer_get_handle(layer);
	orig_handle = M_io_layer_get_handle(orig_layer);

	/* Copy settings */
	M_mem_copy(&handle->settings, &orig_handle->settings, sizeof(handle->settings));

	M_io_layer_release(layer);
	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_bwshaping_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	size_t         max_write = 0;
	M_io_handle_t *handle    = M_io_layer_get_handle(layer);
	M_io_t        *io        = M_io_layer_get_io(layer);
	M_io_error_t   err;
	size_t         request_len;

	/* If latency isn't throttling us, then we see if we are throttle via Bps */
	if (M_io_bwshaping_latency_next_ms(handle, M_IO_BWSHAPING_DIRECTION_OUT) == 0) {
		M_uint64 mymax = M_io_bwshaping_bwtrack_max_size(handle->out_bw, handle->settings.out_Bps, handle->settings.out_period_s,
		                                                 handle->settings.out_sample_frequency_ms, handle->settings.out_mode);
		if (mymax > SIZE_MAX) {
			max_write = SIZE_MAX;
		} else {
			max_write = (size_t)mymax;
		}
	} else {
		/* Haven't hit latency timeout, don't allow write */
		max_write = 0;
	}

	if (max_write > 0) {
		/* Initially disable timers */
		handle->out_waiting = M_FALSE;

		if (*write_len > max_write) {
			/* We're imposing a throttle, we need to set a flag stating we need to enable timers */
			handle->out_waiting = M_TRUE;
			*write_len          = max_write;
		}

		request_len = *write_len;
		err         = M_io_layer_write(io, M_io_layer_get_index(layer)-1, buf, write_len, meta);

		if (err == M_IO_ERROR_SUCCESS) {
			M_io_bwshaping_add_transfer(handle, *write_len, M_IO_BWSHAPING_DIRECTION_OUT);
		}

		/* We can't be throttling if the OS told us we did a partial write or couldn't write at all */
		if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && *write_len < request_len)) {
			handle->out_waiting = (handle->settings.out_latency_ms)?M_TRUE:M_FALSE;
		}
	} else {
		handle->out_waiting = M_TRUE;
		err                 = M_IO_ERROR_WOULDBLOCK;
	}

	M_io_bwshaping_set_timeout(handle);

	return err;
}


static M_io_error_t M_io_bwshaping_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	size_t         max_read = 0;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_error_t   err;
	size_t         request_len;

	/* If latency isn't throttling us, then we see if we are throttle via Bps */
	if (M_io_bwshaping_latency_next_ms(handle, M_IO_BWSHAPING_DIRECTION_IN) == 0) {
		M_uint64 mymax = M_io_bwshaping_bwtrack_max_size(handle->in_bw, handle->settings.in_Bps, handle->settings.in_period_s,
		                                                 handle->settings.in_sample_frequency_ms, handle->settings.in_mode);
		if (mymax > SIZE_MAX) {
			max_read = SIZE_MAX;
		} else {
			max_read = (size_t)mymax;
		}
	} else {
		/* Haven't hit latency timeout, don't allow read */
		max_read = 0;
	}

	if (max_read > 0) {
		/* Set initial waiting state to false */
		handle->in_waiting  = M_FALSE;
		handle->in_fullread = M_TRUE;

		if (*read_len > max_read) {
			/* We're imposing a throttle, we need to set a flag stating we need to enable timers */
			handle->in_waiting  = M_TRUE;
			handle->in_fullread = M_FALSE;
			*read_len           = max_read;
		}

		request_len = *read_len;
		err         = M_io_layer_read(io, M_io_layer_get_index(layer)-1, buf, read_len, meta);

		if (err == M_IO_ERROR_SUCCESS) {
			M_io_bwshaping_add_transfer(handle, *read_len, M_IO_BWSHAPING_DIRECTION_IN);
		} else {
			handle->in_fullread = M_FALSE;
		}

		/* We can't be throttling if the OS told us we did a partial read or couldn't read at all */
		if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && *read_len < request_len)) {
			handle->in_fullread = M_FALSE;
			handle->in_waiting  = (handle->settings.in_latency_ms)?M_TRUE:M_FALSE;
		}
	} else {
		handle->in_waiting = M_TRUE;
		err                = M_IO_ERROR_WOULDBLOCK;
	}

	M_io_bwshaping_set_timeout(handle);
	return err;
}


static M_bool M_io_bwshaping_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint64       to_ms;
	M_bool         retval = M_FALSE;

	switch (*type) {
		case M_EVENT_TYPE_READ:
			/* See if we can read, if not, consume event and reset timer */
			to_ms = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_IN);
			if (to_ms == 0) {
				/* Timer expired, was waiting on timer for reading, deliver event and clear timer.  Timer might fire of course. */
				retval             = M_FALSE; /* Don't consume event */
				handle->in_waiting = M_FALSE; /* Unset that we are waiting */
				M_io_bwshaping_set_timeout(handle);
			} else if (to_ms == M_TIMEOUT_INF) {
				/* Not waiting on a timer for reading, we may or may not actually be able to read,
				 * but the read callback handles this decision and would return WOULDBLOCK if it
				 * violates bandwidth or latency requirements .... pass through */
				retval             = M_FALSE; /* Don't consume event */
			} else {
				/* Timer has not elapsed, need to continue waiting, block event */
				retval             = M_TRUE;  /* consume event, don't deliver */
			}

			return retval;

		case M_EVENT_TYPE_WRITE:
			/* See if we can read, if not, consume event and reset timer */
			to_ms = M_io_bwshaping_timeout_direction(handle, M_IO_BWSHAPING_DIRECTION_OUT);
			if (to_ms == 0) {
				/* Timer expired, was waiting on timer for writing, deliver event and clear timer.  Timer might fire of course. */
				retval              = M_FALSE; /* Don't consume event */
				handle->out_waiting = M_FALSE; /* Unset that we are waiting */
				M_io_bwshaping_set_timeout(handle);
			} else if (to_ms == M_TIMEOUT_INF) {
				/* Not waiting on a timer for writing, we may or may not actually be able to write,
				 * but the write callback handles this decision and would return WOULDBLOCK if it
				 * violates bandwidth or latency requirements .... pass through */
				retval              = M_FALSE; /* Don't consume event */
			} else {
				/* Timer has not elapsed, need to continue waiting, block event */
				retval              = M_TRUE;  /* consume event, don't deliver */
			}

			return retval;

		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_DISCONNECTED:
			M_event_timer_stop(handle->timer);
		default:
			break;
	}

	return M_FALSE;
}


static void M_io_bwshaping_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_event_timer_remove(handle->timer);
	handle->timer = NULL;
}


static M_bool M_io_bwshaping_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return M_FALSE;

	M_mem_set(&handle->starttv, 0, sizeof(handle->starttv));
	M_io_bwshaping_bwtrack_destroy(handle->in_bw);
	handle->in_bw       = NULL;
	M_mem_set(&handle->in_lasttv, 0, sizeof(handle->in_lasttv));
	handle->in_waiting  = M_FALSE;
	handle->in_fullread = M_FALSE;
	M_io_bwshaping_bwtrack_destroy(handle->out_bw);
	handle->out_bw      = NULL;
	M_mem_set(&handle->out_lasttv, 0, sizeof(handle->out_lasttv));
	handle->out_bytes   = 0;
	handle->out_waiting = M_FALSE;

	return M_TRUE;
}


static void M_io_bwshaping_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;

	M_free(handle);
}


M_io_error_t M_io_add_bwshaping(M_io_t *io, size_t *layer_idx)
{
	M_io_handle_t    *handle;
	M_io_layer_t     *layer;
	M_io_callbacks_t *callbacks;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	handle         = M_malloc_zero(sizeof(*handle));

	/* Set some sane defaults */
	handle->settings.in_mode                 = M_IO_BWSHAPING_MODE_BURST;
	handle->settings.in_Bps                  = 0; /* Infinite */
	handle->settings.in_period_s             = 10;
	handle->settings.in_sample_frequency_ms  = 100;
	handle->settings.out_mode                = M_IO_BWSHAPING_MODE_BURST;
	handle->settings.out_Bps                 = 0; /* Infinite */
	handle->settings.out_period_s            = 10;
	handle->settings.out_sample_frequency_ms = 100;

	/* State */
	handle->io     = io;
	handle->in_bw  = M_io_bwshaping_bwtrack_create(handle, M_IO_BWSHAPING_DIRECTION_IN);
	handle->out_bw = M_io_bwshaping_bwtrack_create(handle, M_IO_BWSHAPING_DIRECTION_OUT);
	M_time_elapsed_start(&handle->starttv);

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_bwshaping_init_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_bwshaping_accept_cb);
	M_io_callbacks_reg_read(callbacks, M_io_bwshaping_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_bwshaping_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_bwshaping_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_bwshaping_unregister_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_bwshaping_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_bwshaping_destroy_cb);
	layer = M_io_layer_add(io, M_IO_BWSHAPING_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_idx != NULL)
		*layer_idx = M_io_layer_get_index(layer);

	return M_IO_ERROR_SUCCESS;
}


static void M_io_bwshaping_clear_state(M_io_handle_t *handle, M_io_bwshaping_direction_t direction)
{
	M_io_bwshaping_bwtrack_t *bwtrack;

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		bwtrack = handle->in_bw;
		handle->in_bw->bytes = 0;
	} else {
		bwtrack = handle->out_bw;
		handle->out_bw->bytes = 0;
	}

	M_io_bwshaping_slots_destroy(bwtrack);
	M_io_bwshaping_slots_create(bwtrack, M_io_bwshaping_bwtrack_size(handle, direction));
}


M_bool M_io_bwshaping_set_throttle(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction, M_uint64 Bps)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return M_FALSE;

	handle = M_io_layer_get_handle(layer);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		handle->settings.in_Bps  = Bps;
	} else {
		handle->settings.out_Bps = Bps;
	}

	M_io_bwshaping_clear_state(handle, direction);

	M_io_layer_release(layer);
	return M_TRUE;
}


M_bool M_io_bwshaping_set_throttle_mode(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction, M_io_bwshaping_mode_t mode)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return M_FALSE;

	handle = M_io_layer_get_handle(layer);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		handle->settings.in_mode  = mode;
	} else {
		handle->settings.out_mode = mode;
	}

	M_io_bwshaping_clear_state(handle, direction);

	M_io_layer_release(layer);
	return M_TRUE;
}


M_bool M_io_bwshaping_set_throttle_period(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction, M_uint64 period_s, M_uint64 sample_frequency_ms)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL || period_s == 0 || sample_frequency_ms < 15 || (period_s * 1000) <= sample_frequency_ms) {
		M_io_layer_release(layer);
		return M_FALSE;
	}

	handle = M_io_layer_get_handle(layer);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		handle->settings.in_period_s             = period_s;
		handle->settings.in_sample_frequency_ms  = sample_frequency_ms;
	} else {
		handle->settings.out_period_s            = period_s;
		handle->settings.out_sample_frequency_ms = sample_frequency_ms;
	}

	M_io_bwshaping_clear_state(handle, direction);

	M_io_layer_release(layer);
	return M_TRUE;
}


M_bool M_io_bwshaping_set_latency(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction, M_uint64 latency_ms)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return M_FALSE;

	handle = M_io_layer_get_handle(layer);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		handle->settings.in_latency_ms           = latency_ms;
		M_time_elapsed_start(&handle->in_lasttv);
	} else {
		handle->settings.out_latency_ms          = latency_ms;
		M_time_elapsed_start(&handle->out_lasttv);
	}

	M_io_layer_release(layer);
	return M_TRUE;
}


M_uint64 M_io_bwshaping_get_Bps(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	M_uint64       Bps;
	M_uint64       range_ms;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return 0;

	handle = M_io_layer_get_handle(layer);
	M_io_bwshaping_clean_expired(handle, direction);
	range_ms = M_io_bwshaping_range_ms(handle, direction);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		Bps = (handle->in_bw->bytes * 1000) / range_ms;
	} else {
		Bps = (handle->out_bw->bytes * 1000) / range_ms;
	}

	M_io_layer_release(layer);
	return Bps;
}


M_uint64 M_io_bwshaping_get_totalbytes(M_io_t *io, size_t id, M_io_bwshaping_direction_t direction)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	M_uint64       bytes;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return 0;

	handle = M_io_layer_get_handle(layer);

	if (direction == M_IO_BWSHAPING_DIRECTION_IN) {
		bytes = handle->in_bytes;
	} else {
		bytes = handle->out_bytes;
	}

	M_io_layer_release(layer);
	return bytes;
}


M_uint64 M_io_bwshaping_get_totalms(M_io_t *io, size_t id)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	M_uint64       time_ms;

	layer = M_io_layer_acquire(io, id, M_IO_BWSHAPING_NAME);
	if (layer == NULL)
		return 0;

	handle = M_io_layer_get_handle(layer);

	time_ms = M_time_elapsed(&handle->starttv);

	M_io_layer_release(layer);
	return time_ms;
}
