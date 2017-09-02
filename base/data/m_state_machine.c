/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#include <mstdlib/mstdlib.h>

typedef enum {
	M_STATE_MACHINE_TYPE_UNKNOWN = 0,
	M_STATE_MACHINE_TYPE_SM,
	M_STATE_MACHINE_TYPE_CLEANUP
} M_state_machine_type_t;

typedef enum {
	M_STATE_MACHINE_STATE_TYPE_UNKNOWN = 0,
	M_STATE_MACHINE_STATE_TYPE_FUNC,
	M_STATE_MACHINE_STATE_TYPE_CLEANUP,
	M_STATE_MACHINE_STATE_TYPE_SUBM
} M_state_machine_state_type_t;

typedef struct {
	M_state_machine_state_type_t        type;     /*!< Type of state. */
	M_uint64                            ndescr;   /*!< Numeric description of the state. */
	char                               *descr;    /*!< Textual description of the state. */
	M_list_u64_t                       *next_ids; /*!< Valid ids the state can transition to. NULL means any state. */
	M_state_machine_cleanup_t          *cleanup;  /*!< Cleanup state machine. */
	union {
		struct {
			M_state_machine_state_cb    func;     /*!< The function that is the state. */
		} func;
		struct {
			M_state_machine_cleanup_cb  func;     /*!< The function that is the state for a cleanup sm. */
		} cleanup;
		struct {
			M_state_machine_t          *subm;     /*!< Sub state machine to run. */
			M_state_machine_pre_cb      pre;      /*!< Pre function to run before running the sub state machine. */
			M_state_machine_post_cb     post;     /*!< Post function to run after running the sub state machine. */
		} sub;
	} d;
} M_state_machine_state_t;

struct M_state_machine {
	M_state_machine_type_t            type;              /*!< Type of state machine. */
	M_uint64                          ndescr;            /*!< Numeric description of the state machine. */
	char                             *descr;             /*!< Textual description of the state machine. */
	M_state_machine_flags_t           flags;             /*!< State machine behavior. */
	M_hash_u64vp_t                   *states;            /*!< All of the states the state machine can use. */
	M_list_u64_t                     *state_ids;         /*!< Ordered list of state ids. The order is the order they
	                                                          were inserted into the state machine. */
	M_list_u64_t                     *cleanup_ids;       /*!< A list of state ids that have been run to be called for
	                                                          cleanup. */
	M_hash_u64vp_t                   *cleanup_seen_ids;  /*!< A list of state ids that have already had cleanup been
	                                                          run. */
	M_state_machine_cleanup_reason_t  cleanup_reason;    /*!< The reason cleanup was triggered. */
	M_state_machine_cleanup_reason_t  pcleanup_reason;   /*!< Parent state machine's cleanup reason for cleanup sm. */
	M_state_machine_status_t          return_status;     /*!< The status that triggered cleanup that needs to be
	                                                          returned to the caller once cleanup is finished. */
	M_list_u64_t                     *continuations;     /*!< A list of continuations that have been called since the
	                                                          last successful state. Used to prevent an infinite
	                                                          continuation loop. */
	M_list_u64_t                     *prev_ids;          /*!< A list of ids that have been called. Used for moving
	                                                          backwards when a state specifies it should transition to
	                                                          the previous state. */
	M_uint64                         current_id;         /*!< The current state the machine is running. */
	M_uint64                         current_cleanup_id; /*!< The current cleanup id that's being run. */
	M_bool                           running;            /*!< Is the machine running. If not running a subsequent call
	                                                          to run will rest and start the state machine. */
	M_state_machine_trace_cb         trace_cb;           /*!< Trace callback. */
	void                            *trace_thunk;        /*!< Thunk passed to trace callback. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Forward declaration so muliple functions including itself can call it. */
static M_state_machine_status_t M_state_machine_run_machine(M_state_machine_t *master, M_state_machine_t *current, void *data);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_state_machine_state_t *M_state_machine_state_create(M_state_machine_state_type_t type, M_uint64 ndescr, const char *descr, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	if (type != M_STATE_MACHINE_STATE_TYPE_FUNC && type != M_STATE_MACHINE_STATE_TYPE_CLEANUP && type != M_STATE_MACHINE_STATE_TYPE_SUBM)
		return NULL;

	s           = M_malloc(sizeof(*s));
	s->type     = type;
	s->ndescr   = ndescr;
	s->descr    = M_strdup(descr);
	s->cleanup  = (M_state_machine_cleanup_t *)M_state_machine_duplicate((M_state_machine_t *)cleanup);
	s->next_ids = next_ids;

	return s;
}

static M_state_machine_state_t *M_state_machine_state_create_func(M_uint64 ndescr, const char *descr, M_state_machine_state_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	s              = M_state_machine_state_create(M_STATE_MACHINE_STATE_TYPE_FUNC, ndescr, descr, cleanup, next_ids);
	if (s == NULL)
		return NULL;
	s->d.func.func = func;

	return s;
}

static M_state_machine_state_t *M_state_machine_state_create_cleanup(M_uint64 ndescr, const char *descr, M_state_machine_cleanup_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	s              = M_state_machine_state_create(M_STATE_MACHINE_STATE_TYPE_CLEANUP, ndescr, descr, cleanup, next_ids);
	if (s == NULL)
		return NULL;
	s->d.cleanup.func = func;

	return s;
}

static M_state_machine_state_t *M_state_machine_state_create_subm(M_uint64 ndescr, const char *descr, const M_state_machine_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	s             = M_state_machine_state_create(M_STATE_MACHINE_STATE_TYPE_SUBM, ndescr, descr, cleanup, next_ids);
	if (s == NULL)
		return NULL;
	s->d.sub.subm = M_state_machine_duplicate(subm);
	s->d.sub.pre  = pre;
	s->d.sub.post = post;

	return s;
}

static void M_state_machine_state_destory(M_state_machine_state_t *s)
{
	if (s == NULL)
		return;

	M_free(s->descr);
	s->descr    = NULL;
	M_list_u64_destroy(s->next_ids);
	s->next_ids = NULL;
	M_state_machine_destroy((M_state_machine_t *)s->cleanup);
	s->cleanup = NULL;

	switch (s->type) {
		case M_STATE_MACHINE_STATE_TYPE_UNKNOWN:
			break;
		case M_STATE_MACHINE_STATE_TYPE_FUNC:
			s->d.func.func = NULL;
			break;
		case M_STATE_MACHINE_STATE_TYPE_CLEANUP:
			s->d.cleanup.func = NULL;
			break;
		case M_STATE_MACHINE_STATE_TYPE_SUBM:
			s->d.sub.pre  = NULL;
			s->d.sub.post = NULL;
			M_state_machine_destroy(s->d.sub.subm);
			s->d.sub.subm = NULL;
			break;
	}

	M_free(s);
}

static void M_state_machine_state_destory_vp(void *p)
{
	M_state_machine_state_destory(p);
}

static void M_state_machine_call_trace(M_state_machine_trace_t trace, M_state_machine_t *master, M_state_machine_t *current, M_state_machine_status_t status, M_bool run_sub, M_uint64 next_id)
{
	const char *sdescr  = NULL;
	char       *fdesr   = NULL;
	M_uint64    idx     = 0;
	M_uint64    nsdescr = 0;

	if (master->trace_cb == NULL)
		return;

	M_state_machine_active_state(current, &idx);
	fdesr = M_state_machine_descr_full(master, M_TRUE);

	if (trace != M_STATE_MACHINE_TRACE_MACHINEENTER && trace != M_STATE_MACHINE_TRACE_MACHINEEXIT) {
		sdescr  = M_state_machine_active_state_descr(current, M_FALSE);
		nsdescr = M_state_machine_active_state_ndescr(current, M_FALSE);
	}

	master->trace_cb(trace, current->ndescr, current->descr, nsdescr, sdescr, fdesr, idx, status, run_sub, next_id, master->trace_thunk);

	if (fdesr != NULL)
		M_free(fdesr);
}

/*! Insert an id into the previous id list. */
static void M_state_machine_insert_prev_id(M_state_machine_t *m, M_uint64 id)
{
	if (m == NULL)
		return;
	/* If we're only allowing a single previous then we only store one previous id. */
	if (!(m->flags & M_STATE_MACHINE_SINGLE_PREV) || M_list_u64_len(m->prev_ids) == 0) {
		M_list_u64_insert(m->prev_ids, id);
	} else {
		M_list_u64_replace_at(m->prev_ids, id, 0);
	}
}

/*! Get the id for the last state run. */
static M_uint64 M_state_machine_pop_prev_id(M_state_machine_t *m)
{
	size_t len;

	if (m == NULL)
		return 0;

	len = M_list_u64_len(m->prev_ids);
	if (len == 0)
		return 0;

	return M_list_u64_take_at(m->prev_ids, len-1);
}

/*! Clear the list of ... */
static void M_state_machine_clear_prev_ids(M_state_machine_t *m)
{
	if (m == NULL)
		return;
	M_list_u64_remove_range(m->prev_ids, 0, M_list_u64_len(m->prev_ids));
}

/*! Clear the list of ... */
static void M_state_machine_clear_cleanup_ids(M_state_machine_t *m)
{
	if (m == NULL)
		return;
	M_list_u64_remove_range(m->cleanup_ids, 0, M_list_u64_len(m->cleanup_ids));
	m->current_cleanup_id = 0;

	M_hash_u64vp_destroy(m->cleanup_seen_ids, M_FALSE);
	m->cleanup_seen_ids = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, NULL);
}

/*! Clear the list of ... */
static void M_state_machine_clear_continuations(M_state_machine_t *m)
{
	if (m == NULL)
		return;
	M_list_u64_remove_range(m->continuations, 0, M_list_u64_len(m->continuations));
}

/*! Run though the state's cleanup state machine. */
static M_state_machine_status_t M_state_machine_run_cleanup(M_state_machine_t *master, M_state_machine_t *current, void *data)
{
	M_state_machine_state_t  *state;
	M_state_machine_status_t  status = M_STATE_MACHINE_STATUS_DONE;
	void                     *vp;
	size_t                    len;
	M_uint64                  id;

	if (current == NULL || current->cleanup_reason == M_STATE_MACHINE_CLEANUP_REASON_NONE)
		return M_STATE_MACHINE_STATUS_DONE;

	/* Go through every state that has been seen. This will determine what cleanup state machines need to run. */
	len = M_list_u64_len(current->cleanup_ids);
	while (len-->0) {
		status = M_STATE_MACHINE_STATUS_DONE;
		id     = M_list_u64_take_last(current->cleanup_ids);
		if (id == 0)
			break;

		/* We'll track which id's we've already called cleanup for in case the one_cleanup flag is set. */
		if (current->flags & M_STATE_MACHINE_ONE_CLEANUP) {
			/* Don't cleanup if we've already called cleanup for the state. */
			if (M_hash_u64vp_get(current->cleanup_seen_ids, id, NULL)) {
				continue;
			}
			M_hash_u64vp_insert(current->cleanup_seen_ids, id, NULL);
		}
		if (!M_hash_u64vp_get(current->states, id, &vp)) {
			continue;
		}
		state = vp;
		/* Only cleanup if there is a cleanup function. */
		if (state->cleanup == NULL) {
			continue;
		}

		current->current_cleanup_id = id;
		((M_state_machine_t *)state->cleanup)->pcleanup_reason = current->cleanup_reason;
		status = M_state_machine_run_machine(master, (M_state_machine_t *)state->cleanup, data);
		if (master->trace_cb != NULL) {
			master->trace_cb(M_STATE_MACHINE_TRACE_CLEANUP, current->ndescr, current->descr, state->ndescr, state->descr, NULL, 0, status, M_FALSE, 0, master->trace_thunk);
		}
		((M_state_machine_t *)state->cleanup)->pcleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_NONE;

		switch (status) {
			case M_STATE_MACHINE_STATUS_NEXT:
			case M_STATE_MACHINE_STATUS_PREV:
			case M_STATE_MACHINE_STATUS_CONTINUE:
			case M_STATE_MACHINE_STATUS_WAIT:
				/* Put the id back so when this is called again this cleanup
 				 * machine will run. */
				M_list_u64_insert(current->cleanup_ids, id);
				return status;
			case M_STATE_MACHINE_STATUS_DONE:
				continue;
			case M_STATE_MACHINE_STATUS_STOP_CLEANUP:
			case M_STATE_MACHINE_STATUS_ERROR_STATE:
			case M_STATE_MACHINE_STATUS_ERROR_INVALID:
			case M_STATE_MACHINE_STATUS_ERROR_BAD_ID:
			case M_STATE_MACHINE_STATUS_ERROR_NO_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_NO_PREV:
			case M_STATE_MACHINE_STATUS_ERROR_INF_CONT:
			case M_STATE_MACHINE_STATUS_NONE:
				/* Errors are ignored and not probigated so turn them into done so the next machine will run. */
				status = M_STATE_MACHINE_STATUS_DONE;
				break;
		}

		/* If we're only allowing cleanup run once we need to track that this one ran. */
		if (current->flags & M_STATE_MACHINE_ONE_CLEANUP) {
			M_hash_u64vp_remove(current->cleanup_seen_ids, id, M_FALSE);
		}
	}

	/* All cleanup machines ran so clear the seen states. */
	M_state_machine_clear_cleanup_ids(current);
	return status;
}

static void M_state_machine_descr_append(M_buf_t *buf, const char *descr, M_state_machine_type_t type, M_uint64 id)
{
	if (buf == NULL)
		return;

	if (M_str_isempty(descr))
		descr = "<NULL>";
	
	switch (type) {
		case M_STATE_MACHINE_TYPE_SM:
			M_buf_add_str(buf, "[M] ");
			break;
		case M_STATE_MACHINE_TYPE_CLEANUP:
			M_buf_add_str(buf, "[CM] ");
			break;
		case M_STATE_MACHINE_TYPE_UNKNOWN:
			/* Must be a state. */
			M_buf_add_str(buf, "[S] ");
			break;

	}

	M_buf_add_str(buf, descr);
	if (id != 0) {
		M_buf_add_str(buf, " (");
		M_buf_add_uint(buf, id);
		M_buf_add_byte(buf, ')');
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_state_machine_status_t M_state_machine_run_states(M_state_machine_t *master, M_state_machine_t *current, void *data)
{
	M_state_machine_state_t  *state;
	void                     *vp;
	M_uint64                  next_id;
	size_t                    idx;
	M_bool                    run_sub;
	M_state_machine_status_t  status;

	if (current == NULL)
		return M_STATE_MACHINE_STATUS_ERROR_INVALID;

	/* Do we have states to run? */
	if (M_list_u64_len(current->state_ids) == 0) {
		current->running    = M_FALSE;
		current->current_id = 0;
		return M_STATE_MACHINE_STATUS_DONE;
	}

	/* Start the state machine. */
	if (!current->running) {
		/* Start with the first state if we're not running. */
		M_state_machine_clear_continuations(current);
		M_state_machine_clear_cleanup_ids(current);
		M_state_machine_clear_prev_ids(current);
		current->current_id     = M_list_u64_at(current->state_ids, 0);
		current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_NONE;
		/* We don't reset the pcleanup_reason because it's set before
 		 * the cleanup state machine starts. */
		current->return_status  = M_STATE_MACHINE_STATUS_NONE;
	}
	current->running = M_TRUE;

	while (1) {
		/* Get the state for the current id. */
		vp = NULL;
		if (!M_hash_u64vp_get(current->states, current->current_id, &vp)) {
			/* No state associated with the id. */
			current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
			current->return_status  = M_STATE_MACHINE_STATUS_ERROR_BAD_ID;
		}
		state = vp;

		/* Run thourght cleanup instead of the states if a cleanup reason was set. This indicates there
 		 * was an error (or done) and cleanup should be run. */
		if (current->cleanup_reason != M_STATE_MACHINE_CLEANUP_REASON_NONE) {
			/* Clean up running sub state machines before this one. We want to go all the way down
 			 * and cleanup on the way back up. */
			if (state != NULL && state->type == M_STATE_MACHINE_STATE_TYPE_SUBM && state->d.sub.subm->running) {
				status = M_state_machine_run_machine(master, state->d.sub.subm, data);
				if (status == M_STATE_MACHINE_STATUS_WAIT) {
					return status;
				}
			}

			status = M_state_machine_run_cleanup(master, current, data);
			if (status == M_STATE_MACHINE_STATUS_WAIT)
				return status;
			M_state_machine_clear_cleanup_ids(current);
			current->running = M_FALSE;
			return current->return_status;
		}

		/* Determine which id is next in the linear order of states. */
		next_id = 0;
		/* We only use the linear next auto filling if the state machine doesn't require
 		 * an explicit transition to be set by the current state. */
		if (!(current->flags & M_STATE_MACHINE_EXPLICIT_NEXT)) {
			if (!M_list_u64_index_of(current->state_ids, current->current_id, &idx)) {
				/* Id does not exist in our list of ids so we can't figure out what's next. */
				current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
				current->return_status  = M_STATE_MACHINE_STATUS_ERROR_BAD_ID;
				continue;
			}
			/* Set the id to the next id if it's not last */
			if (idx != M_list_u64_len(current->state_ids)-1) {
				next_id = M_list_u64_at(current->state_ids, idx+1);
			}
		}

		/* Run the state. */
		if (state->type == M_STATE_MACHINE_STATE_TYPE_SUBM) {
			run_sub = M_TRUE;
			/* Call pre if it was set and we haven't called it already. We could have already called it if we
 			 * received a wait from the sub state machine and are calling into it again. */
			if (state->d.sub.pre != NULL && !state->d.sub.subm->running) {
				status = M_STATE_MACHINE_STATUS_CONTINUE;
				M_state_machine_call_trace(M_STATE_MACHINE_TRACE_PRE_START, master, current, M_STATE_MACHINE_STATUS_NONE, M_FALSE, 0);
				run_sub = state->d.sub.pre(data, &status, &next_id);
				M_state_machine_call_trace(M_STATE_MACHINE_TRACE_PRE_FINISH, master, current, status, run_sub, next_id);
			}
			/* The sub state machine may not run based on the result of pre. */
			if (run_sub) {
				/* This sub will run so add the cleanup id to the list.
 				 * We don't do it later when we deal with states because those
				 * will only be added once the state completes (not if it returns wait)
				 * but this might never be done because of sub states underneath but this
				 * still needs to run. */
				if (!state->d.sub.subm->running)
					M_list_u64_insert(current->cleanup_ids, current->current_id);

				/* Run the sub state machine */
				status = M_state_machine_run_machine(master, state->d.sub.subm, data);
				/* If we get a wait we want to forward that up and our next call will be back into the
 				 * sub state machine. */
				if (current->cleanup_reason == M_STATE_MACHINE_CLEANUP_REASON_NONE && status != M_STATE_MACHINE_STATUS_WAIT) {
					if (state->d.sub.post != NULL) {
						M_state_machine_call_trace(M_STATE_MACHINE_TRACE_POST_START, master, current, M_STATE_MACHINE_STATUS_NONE, M_FALSE, 0);
						status = state->d.sub.post(data, status, &next_id);
						M_state_machine_call_trace(M_STATE_MACHINE_TRACE_POST_FINISH, master, current, status, M_FALSE, next_id);
					} else if (status == M_STATE_MACHINE_STATUS_DONE) {
						/* Change STATUS_DONE to STATUS_NEXT so we don't stop this state machine.
 						 * Only the sub state machine is done. */
						status = M_STATE_MACHINE_STATUS_NEXT;
					}
				}
			}
		} else {
			/* If a cleanup reason was passed (not set on the state machine) this means we are in a cleanup state
 			 * machine so call the appropate function. */
			M_state_machine_call_trace(M_STATE_MACHINE_TRACE_STATE_START, master, current, M_STATE_MACHINE_STATUS_NONE, M_FALSE, 0);
			if (current->type == M_STATE_MACHINE_TYPE_SM) {
				status = state->d.func.func(data, &next_id);
			} else {
				status = state->d.cleanup.func(data, current->pcleanup_reason, &next_id);
			}
			M_state_machine_call_trace(M_STATE_MACHINE_TRACE_STATE_FINISH, master, current, status, M_FALSE, next_id);

			/* Internal errors shouldn't be used by states but if they are for some reason treat them as state errors. */
			switch (status) {
				case M_STATE_MACHINE_STATUS_NONE:
				case M_STATE_MACHINE_STATUS_ERROR_INVALID:
				case M_STATE_MACHINE_STATUS_ERROR_BAD_ID:
				case M_STATE_MACHINE_STATUS_ERROR_NO_NEXT:
				case M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT:
				case M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT:
				case M_STATE_MACHINE_STATUS_ERROR_NO_PREV:
				case M_STATE_MACHINE_STATUS_ERROR_INF_CONT:
				case M_STATE_MACHINE_STATUS_STOP_CLEANUP:
				case M_STATE_MACHINE_STATUS_ERROR_STATE:
					status = M_STATE_MACHINE_STATUS_ERROR_STATE;
					break;
				case M_STATE_MACHINE_STATUS_NEXT:
				case M_STATE_MACHINE_STATUS_PREV:
				case M_STATE_MACHINE_STATUS_CONTINUE:
				case M_STATE_MACHINE_STATUS_WAIT:
				case M_STATE_MACHINE_STATUS_DONE:
					break;
			}
		}

		/* State ran so it should cleanup if necessary. */
		if (state->type != M_STATE_MACHINE_STATE_TYPE_SUBM && status != M_STATE_MACHINE_STATUS_WAIT)
			M_list_u64_insert(current->cleanup_ids, current->current_id);

		switch (status) {
			case M_STATE_MACHINE_STATUS_NEXT:
			case M_STATE_MACHINE_STATUS_CONTINUE:
				/* Check that we have a valid transition. */
				if (next_id == 0) {
					/* Explicit next is required or we're not allowing linear done. */
					if (current->flags & M_STATE_MACHINE_EXPLICIT_NEXT || !(current->flags & M_STATE_MACHINE_LINEAR_END)) {
						current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
						current->return_status  = M_STATE_MACHINE_STATUS_ERROR_NO_NEXT;
						continue;
					} else {
						if (current->flags & M_STATE_MACHINE_DONE_CLEANUP) {
							current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_DONE;
							current->return_status  = M_STATE_MACHINE_STATUS_DONE;
							continue;
						}
						M_state_machine_clear_cleanup_ids(current);
						current->running = M_FALSE;
						M_state_machine_clear_continuations(current);
						return M_STATE_MACHINE_STATUS_DONE;
					}
				} else if (state->next_ids != NULL && !M_list_u64_index_of(state->next_ids, next_id, NULL)) {
					current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
					current->return_status  = M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT;
					continue;
				}

				/* Check if we are a continue and we're in an loop. */
				if (status == M_STATE_MACHINE_STATUS_CONTINUE) {
					if (!(current->flags & M_STATE_MACHINE_CONTINUE_LOOP) && M_list_u64_index_of(current->continuations, next_id, NULL)) {
						current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
						current->return_status  = M_STATE_MACHINE_STATUS_ERROR_INF_CONT;
						continue;
					}
					M_list_u64_insert(current->continuations, next_id);
				} else {
					M_state_machine_insert_prev_id(current, current->current_id);
					M_state_machine_clear_continuations(current);
				}

				if (!(current->flags & M_STATE_MACHINE_SELF_CALL) && current->current_id == next_id) {
					current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
					current->return_status  = M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT;
					continue;
				}
				current->current_id = next_id;
			break;

			case M_STATE_MACHINE_STATUS_PREV:
				current->current_id = M_state_machine_pop_prev_id(current);
				/* No previous id to move to */
				if (current->current_id == 0) {
					current->cleanup_reason = M_STATE_MACHINE_CLEANUP_REASON_ERROR;
					current->return_status  = M_STATE_MACHINE_STATUS_ERROR_NO_PREV;
					continue;
				}
			break;

			case M_STATE_MACHINE_STATUS_NONE:
			case M_STATE_MACHINE_STATUS_ERROR_INVALID:
			case M_STATE_MACHINE_STATUS_ERROR_BAD_ID:
			case M_STATE_MACHINE_STATUS_ERROR_NO_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT:
			case M_STATE_MACHINE_STATUS_ERROR_NO_PREV:
			case M_STATE_MACHINE_STATUS_ERROR_INF_CONT:
			case M_STATE_MACHINE_STATUS_STOP_CLEANUP:
			case M_STATE_MACHINE_STATUS_DONE:
			case M_STATE_MACHINE_STATUS_ERROR_STATE:
				if (status != M_STATE_MACHINE_STATUS_DONE || current->flags & M_STATE_MACHINE_DONE_CLEANUP) {
					current->cleanup_reason = (status==M_STATE_MACHINE_STATUS_DONE)?M_STATE_MACHINE_CLEANUP_REASON_DONE:M_STATE_MACHINE_CLEANUP_REASON_ERROR;
					current->return_status  = status;
					M_state_machine_clear_continuations(current);
					continue;
				}
				M_state_machine_clear_cleanup_ids(current);
				current->running = M_FALSE;
				/* Fall through. */
			case M_STATE_MACHINE_STATUS_WAIT:
				M_state_machine_clear_continuations(current);
				return status;
			break;
		}
	}
}

static M_state_machine_status_t M_state_machine_run_machine(M_state_machine_t *master, M_state_machine_t *current, void *data)
{
	M_state_machine_status_t status;
	M_state_machine_call_trace(M_STATE_MACHINE_TRACE_MACHINEENTER, master, current, M_STATE_MACHINE_STATUS_NONE, M_FALSE, 0);
	status = M_state_machine_run_states(master, current, data);
	M_state_machine_call_trace(M_STATE_MACHINE_TRACE_MACHINEEXIT, master, current, M_STATE_MACHINE_STATUS_NONE, M_FALSE, 0);
	return status;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_state_machine_t *M_state_machine_create(M_uint64 ndescr, const char *descr, M_uint32 flags)
{
	M_state_machine_t *m;

	m = M_malloc_zero(sizeof(*m));

	m->type             = M_STATE_MACHINE_TYPE_SM;
	m->ndescr           = ndescr;
	m->descr            = M_strdup(descr);
	m->flags            = flags;
	m->states           = M_hash_u64vp_create(4, 75, M_HASH_U64VP_NONE, M_state_machine_state_destory_vp);
	m->state_ids        = M_list_u64_create(M_LIST_U64_NONE);
	m->cleanup_ids      = M_list_u64_create(M_LIST_U64_NONE);
	m->cleanup_seen_ids = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, NULL);
	m->continuations    = M_list_u64_create(M_LIST_U64_SORTASC);
	m->prev_ids         = M_list_u64_create(M_LIST_U64_NONE);
	m->current_id       = 0;
	m->cleanup_reason   = M_STATE_MACHINE_CLEANUP_REASON_NONE;
	m->return_status    = M_STATE_MACHINE_STATUS_NONE;
	m->running          = M_FALSE;

	return m;
}

void M_state_machine_destroy(M_state_machine_t *m)
{
	if (m == NULL)
		return;

	M_free(m->descr);

	M_hash_u64vp_destroy(m->states, M_TRUE);
	m->states = NULL;

	M_list_u64_destroy(m->state_ids);
	m->state_ids = NULL;

	M_list_u64_destroy(m->cleanup_ids);
	m->cleanup_ids = NULL;

	M_hash_u64vp_destroy(m->cleanup_seen_ids, M_FALSE);
	m->cleanup_seen_ids = NULL;

	M_list_u64_destroy(m->continuations);
	m->continuations = NULL;

	M_list_u64_destroy(m->prev_ids);
	m->prev_ids = NULL;

	m->current_id = 0;

	M_free(m);
}

M_state_machine_cleanup_t *M_state_machine_cleanup_create(M_uint64 ndescr, const char *descr, M_uint32 flags)
{
	M_state_machine_t *cm;

	cm       = M_state_machine_create(ndescr, descr, flags);
	cm->type = M_STATE_MACHINE_TYPE_CLEANUP;
	return (M_state_machine_cleanup_t *)cm;
}

void M_state_machine_cleanup_destroy(M_state_machine_cleanup_t *m)
{
	M_state_machine_destroy((M_state_machine_t *)m);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_state_machine_insert_state(M_state_machine_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, M_state_machine_state_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	if (m == NULL || id == 0 || func == NULL || M_state_machine_has_state(m, id))
		return M_FALSE;

	s = M_state_machine_state_create_func(ndescr, descr, func, cleanup, next_ids);
	if (s == NULL)
		return M_FALSE;
	M_hash_u64vp_insert(m->states, id, s);
	M_list_u64_insert(m->state_ids, id);

	return M_TRUE;
}

M_bool M_state_machine_insert_sub_state_machine(M_state_machine_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_state_t *s;

	if (m == NULL || id == 0 || subm == NULL || M_state_machine_has_state(m, id))
		return M_FALSE;

	s = M_state_machine_state_create_subm(ndescr, descr, subm, pre, post, cleanup, next_ids);
	if (s == NULL)
		return M_FALSE;
	M_hash_u64vp_insert(m->states, id, s);
	M_list_u64_insert(m->state_ids, id);

	return M_TRUE;
}

M_bool M_state_machine_remove_state(M_state_machine_t *m, M_uint64 id)
{
	if (m == NULL || !M_hash_u64vp_remove(m->states, id, M_TRUE))
		return M_FALSE;

	M_list_u64_remove_val(m->state_ids, id, M_LIST_U64_MATCH_VAL);
	return M_TRUE;
}

M_bool M_state_machine_has_state(const M_state_machine_t *m, M_uint64 id)
{
	if (m == NULL)
		return M_FALSE;
	return M_hash_u64vp_get(m->states, id, NULL);
}

const M_list_u64_t *M_state_machine_list_states(const M_state_machine_t *m)
{
	if (m == NULL)
		return NULL;
	return m->state_ids;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_state_machine_cleanup_insert_state(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, M_state_machine_cleanup_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	M_state_machine_t       *sm;
	M_state_machine_state_t *s;

	if (m == NULL || id == 0 || func == NULL || M_state_machine_cleanup_has_state(m, id))
		return M_FALSE;

	sm = (M_state_machine_t *)m;
	s  = M_state_machine_state_create_cleanup(ndescr, descr, func, cleanup, next_ids);
	if (s == NULL)
		return M_FALSE;
	M_hash_u64vp_insert(sm->states, id, s);
	M_list_u64_insert(sm->state_ids, id);

	return M_TRUE;
}

M_bool M_state_machine_cleanup_insert_cleanup_sub_state_machine(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_cleanup_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	return M_state_machine_insert_sub_state_machine((M_state_machine_t *)m, id, ndescr, descr, (const M_state_machine_t *)subm, pre, post, cleanup, next_ids);
}

M_bool M_state_machine_cleanup_insert_sub_state_machine(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids)
{
	return M_state_machine_insert_sub_state_machine((M_state_machine_t *)m, id, ndescr, descr, subm, pre, post, cleanup, next_ids);
}

M_bool M_state_machine_cleanup_remove_state(M_state_machine_cleanup_t *m, M_uint64 id)
{
	return M_state_machine_remove_state((M_state_machine_t *)m, id);
}

M_bool M_state_machine_cleanup_has_state(const M_state_machine_cleanup_t *m, M_uint64 id)
{
	return M_state_machine_has_state((const M_state_machine_t *)m, id);
}

const M_list_u64_t *M_state_machine_cleanup_list_states(const M_state_machine_cleanup_t *m)
{
	return M_state_machine_list_states((const M_state_machine_t *)m);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_state_machine_enable_trace(M_state_machine_t *m, M_state_machine_trace_cb cb, void *thunk)
{
	if (m == NULL)
		return;
	m->trace_cb    = cb;
	m->trace_thunk = thunk;
}

void M_state_machine_cleanup_enable_trace(M_state_machine_cleanup_t *m, M_state_machine_trace_cb cb, void *thunk)
{
	M_state_machine_enable_trace((M_state_machine_t *)m, cb, thunk);
}

M_state_machine_status_t M_state_machine_run(M_state_machine_t *m, void *data)
{
	return M_state_machine_run_machine(m, m, data);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_state_machine_reset(M_state_machine_t *m, M_state_machine_cleanup_reason_t reason)
{
	M_state_machine_state_t *s;
	void                    *vp;

	if (m == NULL || !m->running)
		return;

	if (!M_hash_u64vp_get(m->states, m->current_id, &vp))
		return;

	s = vp;
	if (s->type == M_STATE_MACHINE_STATE_TYPE_SUBM) {
		M_state_machine_reset(s->d.sub.subm, reason);
	} else {
		/* We're at the last state to run. When we run
 		 * the cancel it will go into the clean up ids
		 * and run the cleanup associated with the state.
		 *
		 * If that state has a cleanup sm we need to cancel
		 * it. This will go down the cleanup machine canceling
		 * it's flow. */
		if (M_list_u64_len(m->cleanup_ids) > 0 &&
				M_hash_u64vp_get(m->states, M_list_u64_last(m->cleanup_ids), &vp))
		{
			s = vp;
			if (s != NULL && s->cleanup != NULL) {
				M_state_machine_reset((M_state_machine_t *)s->cleanup, reason);
			}
		}
	}

	if (reason == M_STATE_MACHINE_CLEANUP_REASON_NONE) {
		M_state_machine_clear_cleanup_ids(m);
		M_state_machine_clear_continuations(m);
		M_state_machine_clear_prev_ids(m);
		m->current_id = 0;
		m->running    = M_FALSE;
	}

	m->cleanup_reason = reason;
	m->return_status  = M_STATE_MACHINE_STATUS_DONE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_state_machine_descr_int(const M_state_machine_t *m, M_bool recurse, M_uint64 *ndescr, const char **descr)
{
	const M_state_machine_t *sub;
	M_uint64                  myndescr;
	const char               *mydescr;

	if (ndescr == NULL)
		ndescr = &myndescr;
	if (descr == NULL)
		descr = &mydescr;

	*ndescr = 0;
	*descr  = NULL;

	if (m == NULL)
		return;

	if (!m->running || !recurse) {
		*ndescr = m->ndescr;
		*descr  = m->descr;
		return;
	}

	sub = M_state_machine_active_sub(m, recurse);
	if (sub != NULL) {
		*ndescr = sub->ndescr;
		*descr  = sub->descr;
		return;
	}
	*ndescr = m->ndescr;
	*descr  = m->descr;
}

M_uint64 M_state_machine_ndescr(const M_state_machine_t *m, M_bool recurse)
{
	M_uint64 ndescr = 0;
	M_state_machine_descr_int(m, recurse, &ndescr, NULL);
	return ndescr;
}

const char *M_state_machine_descr(const M_state_machine_t *m, M_bool recurse)
{
	const char *descr = NULL;
	M_state_machine_descr_int(m, recurse, NULL, &descr);
	return descr;
}

const M_state_machine_t *M_state_machine_active_sub(const M_state_machine_t *m, M_bool recurse)
{
	const M_state_machine_state_t *s;
	void                          *vp;

	if (m == NULL || !m->running)
		return NULL;

	if (!M_hash_u64vp_get(m->states, m->current_id, &vp))
		return NULL;
	s = vp;

	if (s->type == M_STATE_MACHINE_STATE_TYPE_SUBM) {
		if (recurse) {
			m = M_state_machine_active_sub(s->d.sub.subm, recurse);
			if (m != NULL) {
				return m;
			}
		}
		return s->d.sub.subm;
	}

	if (m->current_cleanup_id != 0) {
		if (!M_hash_u64vp_get(m->states, m->current_cleanup_id, &vp))
			return NULL;
		s = vp;
		if (recurse) {
			m = M_state_machine_active_sub((M_state_machine_t *)s->cleanup, recurse);
			if (m != NULL) {
				return m;
			}
		}
		return (M_state_machine_t *)s->cleanup;
	}

	return NULL;
}

M_bool M_state_machine_active_state(const M_state_machine_t *m, M_uint64 *id)
{
	if (m == NULL)
		return M_FALSE;
	if (id != NULL)
		*id = 0;

	if (m->running && id != NULL)
		*id = m->current_id;

	return m->running;
}

static void M_state_machine_active_state_descr_int(const M_state_machine_t *m, M_bool recurse, M_uint64 *ndescr, const char **descr)
{
	const M_state_machine_t       *sub;
	const M_state_machine_state_t *s;
	void                          *vp;
	M_uint64                       myndescr;
	const char                    *mydescr;

	if (ndescr == NULL)
		ndescr = &myndescr;
	if (descr == NULL)
		descr = &mydescr;

	*ndescr = 0;
	*descr  = NULL;

	if (m == NULL || !m->running)
		return;

	if (recurse) {
		sub = M_state_machine_active_sub(m, recurse);
		if (sub != NULL) {
			m = sub;
		}
	}

	if (!M_hash_u64vp_get(m->states, m->current_id, &vp))
		return;
	s = vp;

	if (s->type == M_STATE_MACHINE_STATE_TYPE_SUBM && recurse) {
		M_state_machine_active_state_descr_int(s->d.sub.subm, recurse, ndescr, descr);
		return;
	}
	*ndescr = s->ndescr;
	*descr  = s->descr;
}

M_uint64 M_state_machine_active_state_ndescr(const M_state_machine_t *m, M_bool recurse)
{
	M_uint64 ndescr = 0;
	M_state_machine_active_state_descr_int(m, recurse, &ndescr, NULL);
	return ndescr;
}

const char *M_state_machine_active_state_descr(const M_state_machine_t *m, M_bool recurse)
{
	const char *descr = NULL;
	M_state_machine_active_state_descr_int(m, recurse, 0, &descr);
	return descr;
}

char *M_state_machine_descr_full(const M_state_machine_t *m, M_bool show_id)
{
	M_buf_t                 *buf;
	M_state_machine_state_t *s;
	void                    *vp;

	if (m == NULL)
		return NULL;

	buf = M_buf_create();
	do {
		M_state_machine_descr_append(buf, m->descr, m->type, 0);
		M_buf_add_str(buf, " -> ");

		if (!M_hash_u64vp_get(m->states, m->current_id, &vp))
			break;
		s = vp;

		M_state_machine_descr_append(buf, s->descr, M_STATE_MACHINE_TYPE_UNKNOWN, show_id?m->current_id:0);
		M_buf_add_str(buf, " -> ");

		m = M_state_machine_active_sub(m, M_FALSE);
	} while (m != NULL && m->running);

	M_buf_truncate(buf, M_buf_len(buf)-4);
	return M_buf_finish_str(buf, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_state_machine_t *M_state_machine_duplicate(const M_state_machine_t *m)
{
	M_state_machine_t       *dup;
	M_state_machine_state_t *state;
	M_uint64                 id;
	size_t                   len;
	size_t                   i;
	M_bool                   ret = M_FALSE;

	if (m == NULL)
		return NULL;

	if (m->type == M_STATE_MACHINE_TYPE_SM) {
		dup = M_state_machine_create(m->ndescr, m->descr, m->flags);
	} else {
		dup = (M_state_machine_t *)M_state_machine_cleanup_create(m->ndescr, m->descr, m->flags);
	}

	/* Add each state from the state machine into the duplicate state machine. */
	len = M_list_u64_len(m->state_ids);
	for (i=0; i<len; i++) {
		id    = M_list_u64_at(m->state_ids, i);
		state = M_hash_u64vp_get_direct(m->states, id);

		if (state == NULL) {
			M_state_machine_destroy(dup);
			return NULL;
		}

		switch (state->type) {
			case M_STATE_MACHINE_STATE_TYPE_UNKNOWN:
				ret = M_FALSE;
				break;
			case M_STATE_MACHINE_STATE_TYPE_FUNC:
				ret = M_state_machine_insert_state(dup, id, state->ndescr, state->descr, state->d.func.func, state->cleanup, M_list_u64_duplicate(state->next_ids));
				break;
			case M_STATE_MACHINE_STATE_TYPE_CLEANUP:
				ret = M_state_machine_cleanup_insert_state((M_state_machine_cleanup_t *)dup, id, state->ndescr, state->descr, state->d.cleanup.func, state->cleanup, M_list_u64_duplicate(state->next_ids));
				break;
			case M_STATE_MACHINE_STATE_TYPE_SUBM:
				ret = M_state_machine_insert_sub_state_machine(dup, id, state->ndescr, state->descr, state->d.sub.subm, state->d.sub.pre, state->d.sub.post, state->cleanup, M_list_u64_duplicate(state->next_ids));
				break;
		}

		if (!ret) {
			M_state_machine_destroy(dup);
			return NULL;
		}
	}

	return dup;
}
