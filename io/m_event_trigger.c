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

/* Create Dummy Event object.  This is used so we can add softevents to it.
 * It prevents us from having to track an explicit trigger queue, we can
 * use all the standard io layers */

struct M_io_handle {
	M_event_trigger_t *trigger; /* self-reference */
};

static M_bool M_io_event_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}


static M_bool M_io_event_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;

	/* Pass on */
	return M_FALSE;
}


static void M_io_event_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* Though unregistering doesn't need to do anything, really the event loop
	 * may be in its destructor.  If so, after they unregister us, they will
	 * actually destroy us automatically */
}


static void M_io_event_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;

	M_free(handle->trigger);
	M_free(handle);
}


static M_io_state_t M_io_event_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_CONNECTED;
}


static M_io_t *M_io_event_create(M_event_trigger_t *trigger)
{
	M_io_t           *io;
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;

	handle                     = M_malloc_zero(sizeof(*handle));
	handle->trigger            = trigger;
	io                         = M_io_init(M_IO_TYPE_EVENT);
	callbacks                  = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_event_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_event_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_event_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_event_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_event_state_cb);
	M_io_layer_add(io, "TRIGGER", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return io;
}


static void M_io_event_trigger(M_io_t *io)
{
	M_io_layer_t  *layer;

	if (io == NULL || M_io_get_type(io) != M_IO_TYPE_EVENT)
		return;

	layer = M_io_layer_acquire(io, 0, "TRIGGER");
	if (layer == NULL)
		return;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_OTHER, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	/* Since we rely on soft events, we have to wake the event loop */
	M_event_wake(M_io_get_event(io));
}


M_event_trigger_t *M_event_trigger_add(M_event_t *event, M_event_callback_t callback, void *cb_data)
{
	M_event_trigger_t *trigger;

	if (event == NULL)
		return NULL;

	/* Distribute if pool handle was provided */
	event = M_event_distribute(event);

	/* We need to ensure there is a parent wake */
	M_event_lock(event);
	if (event->u.loop.parent_wake == NULL) {
		enum M_EVENT_FLAGS   mask = M_EVENT_FLAG_NOWAKE;
		event->u.loop.parent_wake = M_io_osevent_create(event);
		event->u.loop.flags      &= ~mask;
	}
	M_event_unlock(event);

	trigger     = M_malloc_zero(sizeof(*trigger));
	trigger->io = M_io_event_create(trigger);
	if (trigger->io == NULL) {
		M_free(trigger);
		return NULL;
	}

	M_event_add(event, trigger->io, callback, cb_data);
	return trigger;
}


void M_event_trigger_signal(M_event_trigger_t *trigger)
{
	if (trigger == NULL)
		return;
	M_io_event_trigger(trigger->io);
}


void M_event_trigger_remove(M_event_trigger_t *trigger)
{
	if (trigger == NULL)
		return;
	M_io_destroy(trigger->io);
}


M_bool M_event_trigger_edit_cb(M_event_trigger_t *trigger, M_event_callback_t callback, void *cb_data)
{
	if (trigger == NULL || callback == NULL)
		return M_FALSE;
	return M_event_edit_io_cb(trigger->io, callback, cb_data);
}
