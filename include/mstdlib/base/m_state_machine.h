/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

#ifndef __M_STATE_MACHINE_H__
#define __M_STATE_MACHINE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


/*! \addtogroup m_state_machine State Machine
 *  \ingroup mstdlib_base
 *
 * Non-linear state machine for running a sequence of states.
 *
 * Normally the state machine will run using a linear or linear hybrid method. When adding states to the state machine
 * the order is preserved. By default the next state is the next state added. A state can change/set the next state
 * id. Using the next state is simply a convenience for using the state machine in a linear manner. So a state can
 * set an id to transition to or it can rely on the next state in the ordered state list to be called next. This
 * behavior can be disabled and the state machine will run as a pure non-linear state machine where a transition id
 * is required to be set by a state.
 *
 * States are given an integral id. The id must be unique and is used so a state can specify which state to transition
 * to. No id can use the number zero (0); it is reserved for internal use.
 *
 * Each state can have an optional cleanup state machine. The cleanup will be called when the error or done status
 * are returned by run. All status that ran (including the one that generated the error) will have their cleanup
 * state machine run. The state must have fully completed before it is eligible for its cleanup to run. A machine
 * that has only returned M_STATE_MACHINE_STATUS_WAIT is not eligible for its cleanup to run. A state that is a
 * sub state machine is eligible for cleanup if it is entered (pre does not return M_FALSE).
 *
 * Cleanup is run in reverse order that the states were run in. For example states A - E were run and
 * E returns done. Cleanup for states will run E - A. Due to the state machine supporting non-linear sequences
 * it is possible that a cleanup machines will be called multiple times.
 *
 * Cleanup can be used in multiple ways. They can be resource clean up (particularly useful when the
 * done_cleanup flag is used) that is run when the machine finishes. Or they can be used as error recovery such as
 * performing an action if an error occurs (default use).
 *
 * Cleanup should be specific to the state and should in some way be based on the state they're associated
 * with. A final cleanup on success could be handled as a final state but should be handled outside of
 * the state machine entirely. Such as being handled as part of cleaning up the void pointer of state data.
 *
 * ## Example
 *
 * \code{.c}
 *     typedef enum {
 *         STATE_A = 1,
 *         STATE_B,
 *         STATE_C
 *     } states_t;
 *
 *     static M_state_machine_status_t state_a(void *data, M_uint64 *next)
 *     {
 *         (void)data;
 *         *next = STATE_C;
 *         return M_STATE_MACHINE_STATUS_NEXT;
 *     }
 *
 *     static M_state_machine_status_t state_b(void *data, M_uint64 *next)
 *     {
 *         (void)data;
 *         (void)next;
 *         return M_STATE_MACHINE_STATUS_DONE;
 *     }
 *
 *     static M_state_machine_status_t state_c(void *data, M_uint64 *next)
 *     {
 *         (void)data;
 *         *next = STATE_B;
 *         return M_STATE_MACHINE_STATUS_NEXT;
 *     }
 *
 *     int main(int argc, char **argv)
 *     {
 *         M_state_machine_t        *sm;
 *         M_state_machine_status_t  status;
 *
 *         sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);
 *
 *         M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
 *         M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_b, NULL, NULL);
 *         M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_c, NULL, NULL);
 *
 *         do {
 *             status = M_state_machine_run(sm, NULL);
 *         } while (status == M_STATE_MACHINE_STATUS_WAIT);
 *
 *         if (status != M_STATE_MACHINE_STATUS_DONE) {
 *             M_printf("state machine failure\n");
 *         } else {
 *             M_printf("state machine success\n");
 *         }
 *
 *         M_state_machine_destroy(sm);
 *         return 0;
 *     }
 * \endcode
 *
 *
 *
 * ## Interleaved Example
 *
 * \code{.c}
 * #include <mstdlib/mstdlib.h>
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * 
 * typedef struct {
 *     size_t cnt;
 *     size_t a1_wait_cnt;
 *     size_t a2_wait_cnt;
 *     size_t a3_wait_cnt;
 *     size_t b1_wait_cnt;
 *     size_t b2_wait_cnt;
 *     size_t b3_wait_cnt;
 *     size_t b5_wait_cnt;
 *     size_t c1_wait_cnt;
 *     size_t c2_wait_cnt;
 *     size_t c3_wait_cnt;
 * } data_obj_t;
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * 
 * void trace(M_state_machine_trace_t trace, M_uint64 mndescr, const char *mdescr, M_uint64 sndescr, const char *sdescr, const char *fdescr, M_uint64 id, M_state_machine_status_t status, M_bool run_sub, M_uint64 next_id, void *thunk)
 * {
 *     if (trace == M_STATE_MACHINE_TRACE_STATE_START)
 *         M_printf("STATE: %s\n", fdescr);
 * 
 *     if (trace == M_STATE_MACHINE_TRACE_STATE_FINISH && status == M_STATE_MACHINE_STATUS_WAIT)
 *         M_printf("STATE: %s - WAIT\n", fdescr);
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // CSM
 * 
 * typedef enum {
 *     STATE_CL = 1
 * } state_cl_b_t;
 * 
 * static M_state_machine_status_t state_csm(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
 * {
 *     (void)data;
 *     (void)reason;
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_cleanup_t *create_csm(void)
 * {
 *     M_state_machine_cleanup_t *csm;
 * 
 *     csm = M_state_machine_cleanup_create(0, "CSM", M_STATE_MACHINE_LINEAR_END);
 * 
 *     M_state_machine_cleanup_insert_state(csm, STATE_CL, 0, "CL1", state_csm, NULL, NULL);
 * 
 *     return csm;
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // SM A
 * 
 * typedef enum {
 *     STATE_A1 = 1,
 *     STATE_A2,
 *     STATE_A3
 * } states_a_t;
 * 
 * static M_state_machine_status_t state_a1(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->a1_wait_cnt < 4) {
 *         obj->a1_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->a1_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_a2(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->a2_wait_cnt < 3) {
 *         obj->a2_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->a2_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_a3(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->a3_wait_cnt < 2) {
 *         obj->a3_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->a3_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_t *create_sm_a(void)
 * {
 *     M_state_machine_t *sm;
 *  
 *     sm = M_state_machine_create(0, "SM A", M_STATE_MACHINE_LINEAR_END);
 *  
 *     M_state_machine_insert_state(sm, STATE_A1, 0, "A1", state_a1, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_A2, 0, "A2", state_a2, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_A3, 0, "A3", state_a3, NULL, NULL);
 *  
 *     return sm;
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // SM B
 * 
 * typedef enum {
 *     STATE_B1 = 1,
 *     STATE_B2,
 *     STATE_B3,
 *     STATE_B4,
 *     STATE_B5,
 *     STATE_B6
 * } states_b_t;
 * 
 * static M_state_machine_status_t state_b1(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->b1_wait_cnt < 2) {
 *         obj->b1_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->b1_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_b2(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->b2_wait_cnt < 3) {
 *         obj->b2_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->b2_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_b3(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->b3_wait_cnt < 4) {
 *         obj->b3_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->b3_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_b4(void *data, M_uint64 *next)
 * {
 *     (void)data;
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_b5(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->b5_wait_cnt < 5) {
 *         obj->b5_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->b5_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_b6(void *data, M_uint64 *next)
 * {
 *     (void)data;
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_t *create_sm_b(void)
 * {
 *     M_state_machine_t         *sm;
 *     M_state_machine_cleanup_t *csm;
 *  
 *     sm = M_state_machine_create(0, "SM B", M_STATE_MACHINE_LINEAR_END|M_STATE_MACHINE_DONE_CLEANUP);
 *  
 *     csm = create_csm();
 *     M_state_machine_insert_state(sm, STATE_B1, 0, "B1", state_b1, csm, NULL);
 *     M_state_machine_insert_state(sm, STATE_B1, 0, "B1", state_b1, NULL, NULL);
 *     M_state_machine_cleanup_destroy(csm);
 *     M_state_machine_insert_state(sm, STATE_B2, 0, "B2", state_b2, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_B3, 0, "B3", state_b3, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_B4, 0, "B4", state_b4, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_B5, 0, "B5", state_b5, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_B6, 0, "B6", state_b6, NULL, NULL);
 *  
 *     return sm;
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // SM C
 * 
 * typedef enum {
 *     STATE_C1 = 1,
 *     STATE_C2,
 *     STATE_C3
 * } states_c_t;
 * 
 * static M_state_machine_status_t state_c1(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->c1_wait_cnt < 4) {
 *         obj->c1_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->c1_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_c2(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->c2_wait_cnt < 0) {
 *         obj->c2_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->c2_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_c3(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     (void)next;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->c3_wait_cnt < 1) {
 *         obj->c3_wait_cnt++;
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *     obj->c3_wait_cnt = 0;
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_t *create_sm_c(void)
 * {
 *     M_state_machine_t         *sm;
 *     M_state_machine_cleanup_t *csm;
 *  
 *     sm = M_state_machine_create(0, "SM C", M_STATE_MACHINE_LINEAR_END|M_STATE_MACHINE_DONE_CLEANUP);
 *  
 *     csm = create_csm();
 *     M_state_machine_insert_state(sm, STATE_C1, 0, "C1", state_c1, csm, NULL);
 *     M_state_machine_cleanup_destroy(csm);
 *     M_state_machine_insert_state(sm, STATE_C2, 0, "C2", state_c2, NULL, NULL);
 *     M_state_machine_insert_state(sm, STATE_C3, 0, "C3", state_c3, NULL, NULL);
 *  
 *     return sm;
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // SM MAIN
 * 
 * typedef enum {
 *     STATE_MA = 1,
 *     STATE_MB, // Interleaved state.
 *     STATE_MC
 * } states_m_t;
 * 
 * static M_state_machine_status_t state_ma(void *data, M_uint64 *next)
 * {
 *     M_printf("Calling func: %s\n", __func__);
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_bool inter_pre(void *data, M_state_machine_status_t *status, M_uint64 *next)
 * {
 *     (void)data;
 *     (void)status;
 *     (void)next;
 *     M_printf("Calling func: %s\n", __func__);
 *     return M_TRUE;
 * }
 * 
 * static M_state_machine_status_t inter_post(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
 * {
 *     (void)data;
 *     (void)next;
 * 
 *     M_printf("Calling func: %s with sub status %u\n", __func__, sub_status);
 * 
 *     if (sub_status != M_STATE_MACHINE_STATUS_DONE)
 *         return sub_status;
 * 
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_status_t state_mc(void *data, M_uint64 *next)
 * {
 *     data_obj_t *obj = data;
 * 
 *     M_printf("Calling func: %s\n", __func__);
 * 
 *     if (obj->cnt < 3) {
 *         *next = STATE_MA;
 * 
 *         obj->cnt++;
 *         M_printf("Going back to main cnt: %zu\n", obj->cnt);
 *     }
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * 
 * static M_state_machine_t *create_sm_m(void)
 * {
 *     M_state_machine_t *sm;
 *     M_state_machine_t *subm;
 *  
 *     sm = M_state_machine_create(0, "SM M", M_STATE_MACHINE_LINEAR_END|M_STATE_MACHINE_INTERNOABORT);
 *  
 *     M_state_machine_insert_state(sm, STATE_MA, 0, "SA", state_ma, NULL, NULL);
 * 
 *     M_state_machine_insert_state_interleaved(sm, STATE_MB, 0, "SIB", inter_pre, inter_post, NULL, NULL);
 *     subm = create_sm_a();
 *     M_state_machine_insert_sub_state_machine_interleaved(sm, STATE_MB, subm);
 *     M_state_machine_destroy(subm);
 *     subm = create_sm_b();
 *     M_state_machine_insert_sub_state_machine_interleaved(sm, STATE_MB, subm);
 *     M_state_machine_destroy(subm);
 *     subm = create_sm_c();
 *     M_state_machine_insert_sub_state_machine_interleaved(sm, STATE_MB, subm);
 *     M_state_machine_destroy(subm);
 * 
 *     M_state_machine_insert_state(sm, STATE_MC, 0, "SC", state_mc, NULL, NULL);
 *  
 *     return sm;
 * }
 * 
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * 
 * int main(int argc, char **argv)
 * {
 *     M_state_machine_t        *sm;
 *     data_obj_t               *obj;
 *     M_state_machine_status_t  status;
 * 
 *     sm  = create_sm_m();
 *     obj = M_malloc_zero(sizeof(*obj));
 *     M_state_machine_enable_trace(sm, trace, NULL);
 *  
 *     do {
 *         status = M_state_machine_run(sm, obj);
 *     } while (status == M_STATE_MACHINE_STATUS_WAIT);
 *  
 *     if (status != M_STATE_MACHINE_STATUS_DONE) {
 *         M_printf("state machine failure\n");
 *     } else {
 *         M_printf("state machine success\n");
 *     }
 *  
 *     M_state_machine_destroy(sm);
 *     M_free(obj);
 *     return 0;
 * }
 * \endcode
 *
 * @{
 */

struct M_state_machine;
typedef struct M_state_machine M_state_machine_t;

struct M_state_machine_cleanup;
typedef struct M_state_machine_cleanup M_state_machine_cleanup_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Status of the state machine while running though states. */
typedef enum {
	/* State specific status. Only states can return these. */
	M_STATE_MACHINE_STATUS_NONE = 0,        /*!< Invalid status. */
	M_STATE_MACHINE_STATUS_NEXT,            /*!< Success continue to the next state.
	                                             The state was run and should be recored as well as cleanup added
	                                             to the cleanup list. */
	M_STATE_MACHINE_STATUS_PREV,            /*!< A recoverable error occurred. Go to the last successful (non-continue)
	                                             state.

	                                             This should be treated as a special case and is primarily a
	                                             convenience when using the state machine in a linear manner. It should
	                                             not be used in stead of specifying an id and calling next if it is
	                                             possible to do so.

										     	 This does not back out states. State cleanups will not be called when
	                                             skipping back over states. Also, the list of cleanups will not be
	                                             modified to remove cleanups for states that have been called. Further,
	                                             This can result in a state having it's cleanup registered multiple
	                                             times as a result of multiple successful calls. */
	M_STATE_MACHINE_STATUS_CONTINUE,        /*!< Success continue to the next state.
	                                             The state was skipped and should be treated as such. The cleanup
	                                             for this state will not be added to the cleanup list.

	                                             This should not be treated as next without cleanup. It is for
	                                             signifying that the state was skipped. If you need next without
	                                             cleanup the state should be registered without a cleanup state
	                                             machine. Even if that means having two ids for the same state function
	                                             one with and one without a cleanup registered. */

	/* Shared status. States and the state machine can return these. */
	M_STATE_MACHINE_STATUS_ERROR_STATE,     /*!< An unrecoverable error occurred within a state. Exit and clean up.
	                                             The state is responsible for error reporting though the void data
	                                             pointer passed to the state function. */
	M_STATE_MACHINE_STATUS_WAIT,            /*!< The state is processing in a non-blocking fashion. More calls to run
	                                             are required to continue the operation. */
	M_STATE_MACHINE_STATUS_DONE,            /*!< The sequence completed successfully. */

	/* State machine specific status. Only the state machine can return these. */
	M_STATE_MACHINE_STATUS_STOP_CLEANUP,     /*< Used by cleanup state machines to stop processing further cleanup
	                                             state machines within a state machine. */
 	/* All of these are unrecoverable errors. */
	M_STATE_MACHINE_STATUS_ERROR_INVALID,   /*!< The state machine was called with an invalid parameter. */
	M_STATE_MACHINE_STATUS_ERROR_BAD_ID,    /*!< Invalid transition specified. Id not found. Most likely the state
	                                             specified an id to transition to that doesn't exist. */
	M_STATE_MACHINE_STATUS_ERROR_NO_NEXT,   /*!< Invalid transition specified. An next id was not specified. This can
	                                             happen when running in a linear manner and the last state in the
	                                             sequence does not return done. There are no states after the last
	                                             state so we cannot continue with the sequence. */
	M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT,  /*!< Invalid transition specified. The specified next id is not valid (not
	                                             listed in the states list of next ids) for the state. */
	M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT, /*!< Invalid transition specified. The specified next id is the current id.
	                                             Use the continue_loop flag to disable this check. */
	M_STATE_MACHINE_STATUS_ERROR_NO_PREV,   /*!< Invalid transition specified. There are no previous states to
	                                             transition to. */
	M_STATE_MACHINE_STATUS_ERROR_INF_CONT   /*!< A possible infinite continuation loop has been encountered. */
} M_state_machine_status_t;


/*! Options to control the behavior of the state machine. */
typedef enum {
	M_STATE_MACHINE_NONE          = 0,      /*!< Normal operation. */
	M_STATE_MACHINE_SINGLE_PREV   = 1 << 1, /*!< Do not allow multiple states to return STATUS_PREV in a row.
	                                             Only one PREV return is allowed between NEXT calls. */
	M_STATE_MACHINE_CONTINUE_LOOP = 1 << 2, /*!< Normally continuations are tracked for the continuation cycle
	                                             and any continuation that is repeated is treated as an internal
	                                             error in order to detect and prevent accidental infinite loops.

	                                             This option disables this check and allows continuations to call
	                                             continuations that have been called previously. */
	M_STATE_MACHINE_SELF_CALL     = 1 << 3, /*!< Normally states cannot call themselves. This flag also
	                                             allows states to call themselves. */
	M_STATE_MACHINE_DONE_CLEANUP  = 1 << 4, /*!< State cleanups should be called on done. */
	M_STATE_MACHINE_ONE_CLEANUP   = 1 << 5, /*!< State cleanup should be called once no matter how many times the
	                                             state was called. */
	M_STATE_MACHINE_EXPLICIT_NEXT = 1 << 6, /*!< Normally the state machine defaults to using the next state in
	                                             the order states were added if a state isn't explicits specified
	                                             by the current state. This requires that a state specify the next
	                                             (transition) state.

	                                             This will force the state machine to function purely as a non-linear
	                                             state machine. The linear / linear hybrid functionality will be
	                                             disabled. This option cannot be used in conjunction with linear_end.
	                                             The linear_end flag will be ignored if this flag is set. */
	M_STATE_MACHINE_LINEAR_END    = 1 << 7, /*!< Normally a state machine is done when the done status is returned
	                                             by a state. This allows the state machine to be considered done if
	                                             a state does not specify a transition, it returns next or continue
	                                             and the current state is the last state in the ordered state list. */
	M_STATE_MACHINE_INTERNOABORT  = 1 << 8  /*!< Interleaved sub state machines should continue processing until done
	                                             even when another sub state machine errors. Prevents aborting other
	                                             interleaved sub state machines from aborting. An error will still
	                                             be returned as the result of the interleaved state (sent to the post
	                                             callback) but only after all sub state machines have finished. If
	                                             multiple sub state machines failed, the error status for the first
	                                             added will be used. */
} M_state_machine_flags_t;


/*! Status of the state machine which caused the cleanup routines to trigger. */
typedef enum {
	M_STATE_MACHINE_CLEANUP_REASON_NONE = 0, /*!< Cleanup should not be run. When calling reset this will not run
	                                              cleanup. */
	M_STATE_MACHINE_CLEANUP_REASON_DONE,     /*!< State machine finished successfully. */
	M_STATE_MACHINE_CLEANUP_REASON_ERROR,    /*!< State machine stopped due to error. */
	M_STATE_MACHINE_CLEANUP_REASON_RESET,    /*!< State machine should be reset so it can run again. This is a reason
	                                              why cleanup is being run. */
	M_STATE_MACHINE_CLEANUP_REASON_CANCEL    /*!< State machine was canceled. This will reset the machine so it can
	                                              run again but should be considered that it will not be run again.
	                                              Use reset for restarting instead. */
} M_state_machine_cleanup_reason_t;


/*! Tracing information. */
typedef enum {
	M_STATE_MACHINE_TRACE_NONE = 0,     /*!< Invalid. */
	M_STATE_MACHINE_TRACE_MACHINEENTER, /*!< About to enter a given state machine (could be sub)
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
	                                         * fdescr */
	M_STATE_MACHINE_TRACE_MACHINEEXIT,  /*!< Machine exited.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
	                                         * fdescr
	                                         * status */
	M_STATE_MACHINE_TRACE_STATE_START,  /*!< State is about to run.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id
	                                         */
	M_STATE_MACHINE_TRACE_STATE_FINISH, /*!< State finished running.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id
	                                         * next_id
	                                         * status */
	M_STATE_MACHINE_TRACE_PRE_START,    /*!< Pre function will run before entering a sub machine.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id
	                                         */
	M_STATE_MACHINE_TRACE_PRE_FINISH,   /*!< Pre functoin finished running.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id
	                                         * run_sub
	                                         * status */
	M_STATE_MACHINE_TRACE_POST_START,   /*!< Sub machine finished but before post function runs.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id */
	M_STATE_MACHINE_TRACE_POST_FINISH,  /*!< Sub machine finished running but after post function ran.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr
	                                         * fdescr
	                                         * id
	                                         * status */
	M_STATE_MACHINE_TRACE_CLEANUP       /*!< Cleanup function ran.
	                                         Will provide the following information:
											 * mndescr
	                                         * mdescr
											 * sndescr
											 * sdescr */
} M_state_machine_trace_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Trace callback.
 *
 * \param[in] trace   Type of action traced.
 * \param[in] mndescr Numeric state machine description code.
 * \param[in] mdescr  State machine description.
 * \param[in] sndescr Numeric state description code.
 * \param[in] sdescr  State description.
 * \param[in] fdescr  Full description of the entire machine flow.
 * \param[in] id      Id of state.
 * \param[in] status  Return status.
 * \param[in] run_sub Will the sub state machine be run.
 * \param[in] next_id The next id the machine will move to.
 * \param[in] thunk   Thunk passed in when enabling the trace.
 */
typedef void (*M_state_machine_trace_cb)(M_state_machine_trace_t trace, M_uint64 mndescr, const char *mdescr, M_uint64 sndescr, const char *sdescr, const char *fdescr, M_uint64 id, M_state_machine_status_t status, M_bool run_sub, M_uint64 next_id, void *thunk);


/*! State callback.
 *
 * This is what the state machine calls when entering a given state.
 *
 * \param[in,out] data An opaque data type storing data that should be passed to the cb.
 * \param[out]    next The next id the state machine should transition to. When operating in linear or a hybrid manner
 *                     this will be set to the next linear state. Changing this will change what state is next.
 *
 * \return The status.
 */
typedef M_state_machine_status_t (*M_state_machine_state_cb)(void *data, M_uint64 *next);


/*! Cleanup state callback.
 *
 * This is what a cleanup state machine calls when entering a given cleanup state.
 *
 * \param[in,out] data   An opaque data type storing data that should be passed to the cb.
 * \param[in]     reason The reason cleanup is being run.
 * \param[out]    next   The next id the state machine should transition to. When operating in linear or a hybrid manner
 *                       this will be set to the next linear state. Changing this will change what state is next.
 *
 * \return The status.
 */
typedef M_state_machine_status_t (*M_state_machine_cleanup_cb)(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next);


/*! Sub state machine pre (initialization) callback.
 *
 * This will be called before starting a sub state machine.
 *
 * \param[in,out] data     An opaque data type storing data that should be passed to the cb.
 * \param[out]    status   Used when not running the sub state machine. This is the status of the state. Defaults
 *                         to M_STATE_MACHINE_STATUS_NEXT if not specified.
 * \param[out]    next     Used when not running the sub state machine. If set the next id the state machine should
 *                         transition to. When operating in linear or a hybrid manner this will be set to the next
 *                         linear state. Changing this will change what state is next.
 *
 * \return M_TRUE if the sub state machine should run. M_FALSE if the sub state machine should not run.
 */
typedef M_bool (*M_state_machine_pre_cb)(void *data, M_state_machine_status_t *status, M_uint64 *next);


/*! Sub state machine post (de-initialization) callback.
 *
 * The sub_status argument is the status returned by the sub state machine. Possible status:
 *
 * - M_STATE_MACHINE_STATUS_DONE
 * - M_STATE_MACHINE_STATUS_ERROR_*
 *
 * The sub_status will next be M_STATE_MACHINE_STATUS_NEXT or similar. Thus,
 * the sub_status should not be blindly returned from the post function as it
 * will stop processing the parent state machine. If processing needs to
 * continue the sub_status should be checked and M_STATE_MACHINE_STATUS_NEXT or
 * similar should be returned. M_STATE_MACHINE_STATUS_DONE is the only
 * successful sub_status that can be set, so patterns that check against
 * M_STATE_MACHINE_STATUS_DONE should be used. For example:
 *
 * \code{c}
 * if (sub_status == M_STATE_MACHINE_STATUS_DONE) {
 *     // Success and continue.
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 * ...
 * \endcode
 *
 * \code{c}
 * if (sub_status != M_STATE_MACHINE_STATUS_DONE) {
 *     // Error of some kind. Propagate it up.
 *     return sub_status;
 * }
 * ...
 * \endcode
 *
 * \code{c}
 * if (stop_condition) {
 *    // Some kind of external stop condition was encountered.
 *    // Return the sub_status because it will stop processing
 *    // and we should maintain status from the sub state machine.
 *    return sub_status; // Status is error or done.
 * }
 * ...
 * \endcode
 *
 * \param[in,out] data       An opaque data type storing data that should be passed to the cb.
 * \param[out]    sub_status The status of the last state in the sub state machine.
 * \param[out]    next       The next id the state machine should transition to. When operating in linear or a hybrid manner
 *                           this will be set to the next linear state. Changing this will change what state is next.
 *
 * \return The status.
 */
typedef M_state_machine_status_t (*M_state_machine_post_cb)(void *data, M_state_machine_status_t sub_status, M_uint64 *next);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new state machine.
 *
 * \param[in] ndescr A numeric description of the state machine. Can be 0.
 * \param[in] descr  A textual description of the state machine. Can be NULL.
 * \param[in] flags  M_state_machine_flags_t flags to control the behavior of the state machine.
 *
 * \return The state machine.
 */
M_API M_state_machine_t *M_state_machine_create(M_uint64 ndescr, const char *descr, M_uint32 flags);


/*! Destroy a state machine.
 *
 * This does not call the cleanup state machines associated with each state. State cleanups are only called when the
 * state machine finishes running.
 *
 * \param[in] m The state machine.
 */
M_API void M_state_machine_destroy(M_state_machine_t *m);


/*! Create a new cleanup state machine.
 *
 * A cleanup state machine is very similar to a regular state machine and is only called when associated with
 * a regular state machine state's cleanup parameter. This cannot be run directly but supports all options a
 * regular state machine supports for execution.
 *
 * When run error returns from a cleanup state machine will not be propagated back to the caller.
 * To handle errors it is possible to have a cleanup state machine's state to have an associated cleanup
 * state machine.
 *
 * \param[in] ndescr A numeric description of the cleanup state machine. Can be 0.
 * \param[in] descr  A textual description of the cleanup state machine. Can be NULL.
 * \param[in] flags  M_state_machine_flags_t flags to control the behavior of the cleanup state machine.
 *
 * \return The cleanup state machine.
 */
M_API M_state_machine_cleanup_t *M_state_machine_cleanup_create(M_uint64 ndescr, const char *descr, M_uint32 flags);


/*! Destroy a cleanup state machine.
 *
 * \param[in] m The cleanup state machine.
 */
M_API void M_state_machine_cleanup_destroy(M_state_machine_cleanup_t *m);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add a state to the state machine.
 *
 * \param[in,out] m        The state machine.
 * \param[in]     id       The id associated with this state. Must be unique.
 * \param[in]     ndescr   A numeric description of the state. Can be 0.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     func     The state function to call. Cannot be NULL.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the state was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_insert_state(M_state_machine_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, M_state_machine_state_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Add a state machine as a state to the state machine.
 *
 * The state machine will duplicate the sub state machine and keep a copy.
 *
 * The sub state machine will run though all states in the sub state machine. The state machine will return
 * M_STATE_MACHINE_STATUS_WAIT from the sub state machine and resume the sub state machine when started again.
 *
 * The sub state machine's final status will be passed to the post function if one is given. If a post function is
 * not set, a status of M_STATE_MACHINE_STATUS_DONE will be returned as M_STATE_MACHINE_STATUS_NEXT. This
 * is to prevent a M_STATE_MACHINE_STATUS_DONE from the sub state machine from accidentally stopping the calling
 * state machine. If M_STATE_MACHINE_STATUS_DONE is needed as the result of the sub state machine's run then a
 * post function is necessary.
 *
 * \param[in,out] m        The state machine.
 * \param[in]     id       The id associated with this state. Must be unique.
 * \param[in]     ndescr   A numeric description of the state. Can be 0.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     subm     The state machine that should be called from this one. Cannot be NULL.
 * \param[in]     pre      A function to call before the sub state machine is started. Can be NULL.
 * \param[in]     post     A function to call after the sub state machine is finished. Can be NULL.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the sub state machine was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_insert_sub_state_machine(M_state_machine_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Add add a state to run interleaved sub state machines.
 *
 * An interleaved state will have one or more sub state machines added to it.
 * The sub state machines will be run interleaved as one returns wait the next
 * will be started until it returns wait and so forth. Sub state machines process
 * in a non liner order and do not match state counts between one another.
 *
 * A single state machine that never returns wait will fully run before any others
 * are. Interleaving relies on wait states. Typically, this is used for near concurrent
 * processing for operations that may require waiting on external resources. For example,
 * a system that receives, processes and sends data. A example flow: read -> process ->
 * Interleave write and read -> process... This allows the read buffer to be filled while
 * the write buffer is emptying. With a large amount of data read and write operations
 * may need to wait for OS level network buffers.
 *
 * States are run interleaved and _not_ concurrently. They are run on the same
 * thread. Locking of thunk resources is not necessary since only one state,
 * regardless of sub state machine, will be running at any given time.
 *
 * All running sub state machines will stop if an error is returned by any sub state machine.
 * Unless the `M_STATE_MACHINE_INTERNOABORT` flag is set which will alter this behavior.
 *
 * \param[in,out] m        The state machine.
 * \param[in]     id       The id associated with this interleaved state. Must be unique.
 *                         All interleaved sub state machines will attach to this id.
 * \param[in]     ndescr   A numeric description of the state. Can be 0.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     pre      A function to call before the sub state machines are started. Can be NULL.
 * \param[in]     post     A function to call after the sub state machines are finished. Can be NULL.
 *                         Called when the last sub state machine is finished or they are aborted due to error.
 *                         Will have an `M_STATE_MACHINE_STATUS_ERROR_*` if any of the sub state machines return
 *                         an error status. If all sub state machines ran successfully will return
 *                         `M_STATE_MACHINE_STATUS_DONE`. It is up to the caller to store information in the
 *                         `thunk` argument passed to run if information about which (possibly multiple) sub
 *                         state machines failed.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the interleaved state was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_insert_state_interleaved(M_state_machine_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Add a state machine as a state to the state machine for interleaved processing.
 *
 * The state machine will duplicate the sub state machine and keep a copy.
 *
 * \param[in,out] m    The state machine.
 * \param[in]     id   The id for the interleaved state as specified by `M_state_machine_insert_state_interleaved`.
 * \param[in]     subm The state machine that should be called from this one. Cannot be NULL.
 *
 * \return M_TRUE if the sub state machine was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_insert_sub_state_machine_interleaved(M_state_machine_t *m, M_uint64 id, const M_state_machine_t *subm);


/*! Remove a state from the state machine.
 *
 * \param[in,out] m  The state machine.
 * \param[in]     id The id of the state.
 *
 * \return M_TRUE if the state was found and removed. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_remove_state(M_state_machine_t *m, M_uint64 id);


/*! Does the state machine contain the given state id.
 *
 * \param[in] m  The state machine.
 * \param[in] id The id of the state.
 *
 * \return M_TRUE if the state machine has the state id. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_has_state(const M_state_machine_t *m, M_uint64 id);


/*! List all state ids the state machine holds.
 *
 * \param[in] m The state machine.
 *
 * \return a List of state ids or NULL if the state machine as no states.
 */
M_API const M_list_u64_t *M_state_machine_list_states(const M_state_machine_t *m);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add a cleanup state to a cleanup state machine.
 *
 * \param[in,out] m        The cleanup state machine.
 * \param[in]     id       The id associated with this state. Must be unique.
 * \param[in]     ndescr   A numeric description of the state. Can be 0.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     func     The state cleanup function to call. Cannot be NULL.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the state was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_cleanup_insert_state(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, M_state_machine_cleanup_cb func, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Add a cleanup state machine as a state to the cleanup state machine.
 *
 * The state machine will duplicate the sub state machine and keep a copy.
 *
 * The sub state machine will run though all states in the sub state machine. The state machine will return
 * M_STATE_MACHINE_STATUS_WAIT from the sub state machine and resume the sub state machine when started again.
 *
 * The sub state machine's final status will be passed to the post function if one is given. If a post function is
 * not set, a status of M_STATE_MACHINE_STATUS_DONE will be returned as M_STATE_MACHINE_STATUS_NEXT. This
 * is to prevent a M_STATE_MACHINE_STATUS_DONE from the sub state machine from accidentally stopping the calling
 * state machine. If M_STATE_MACHINE_STATUS_DONE is needed as the result of the sub state machine's run then a
 * post function is necessary.
 *
 * \param[in,out] m        The cleanup state machine.
 * \param[in]     id       The id associated with this state. Must be unique.
 * \param[in]     ndescr   A numeric description of the state. Can be 0.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     subm     The cleanup state machine that should be called from this one. Cannot be NULL.
 * \param[in]     pre      A function to call before the sub state machine is started. Can be NULL.
 * \param[in]     post     A function to call after the sub state machine is finished. Can be NULL.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the sub state machine was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_cleanup_insert_cleanup_sub_state_machine(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_cleanup_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Add a state machine as a state to the cleanup state machine.
 *
 * The state machine will duplicate the sub state machine and keep a copy.
 *
 * The sub state machine will run though all states in the sub state machine. The state machine will return
 * M_STATE_MACHINE_STATUS_WAIT from the sub state machine and resume the sub state machine when started again.
 *
 * The sub state machine's final status will be passed to the post function if one is given. If a post function is
 * not set, a status of M_STATE_MACHINE_STATUS_DONE will be returned as M_STATE_MACHINE_STATUS_NEXT. This
 * is to prevent a M_STATE_MACHINE_STATUS_DONE from the sub state machine from accidentally stopping the calling
 * state machine. If M_STATE_MACHINE_STATUS_DONE is needed as the result of the sub state machine's run then a
 * post function is necessary.
 *
 * \param[in,out] m        The cleanup state machine.
 * \param[in]     id       The id associated with this state. Must be unique.
 * \param[in]     ndescr   A numeric description of the state. Can be NULL.
 * \param[in]     descr    A textual description of the state. Can be NULL.
 * \param[in]     subm     The state machine that should be called from this one. Cannot be NULL.
 * \param[in]     pre      A function to call before the sub state machine is started. Can be NULL.
 * \param[in]     post     A function to call after the sub state machine is finished. Can be NULL.
 * \param[in]     cleanup  The cleanup state machine to call. Can be NULL if no cleanup is necessary for this state.
 * \param[in]     next_ids A list of valid transitions for this state. Can be NULL to denote all states are
 *                         valid transitions. If not NULL the state machine takes ownership of next_ids.
 *
 * \return M_TRUE if the sub state machine was added. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_cleanup_insert_sub_state_machine(M_state_machine_cleanup_t *m, M_uint64 id, M_uint64 ndescr, const char *descr, const M_state_machine_t *subm, M_state_machine_pre_cb pre, M_state_machine_post_cb post, M_state_machine_cleanup_t *cleanup, M_list_u64_t *next_ids);


/*! Remove a state from the cleanup state machine.
 *
 * \param[in,out] m  The state machine.
 * \param[in]     id The id of the state.
 *
 * \return M_TRUE if the state was found and removed. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_cleanup_remove_state(M_state_machine_cleanup_t *m, M_uint64 id);


/*! Does the cleanup state machine contain the given state id.
 *
 * \param[in] m  The cleanup state machine.
 * \param[in] id The id of the state.
 *
 * \return M_TRUE if the state machine has the state id. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_cleanup_has_state(const M_state_machine_cleanup_t *m, M_uint64 id);


/*! List all state ids the cleanup state machine holds.
 *
 * \param[in] m The cleanup state machine.
 *
 * \return a List of state ids or NULL if the state machine as no states.
 */
M_API const M_list_u64_t *M_state_machine_cleanup_list_states(const M_state_machine_cleanup_t *m);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Enabling tracing of state machine flow.
 *
 * \param[in] m     The state machine.
 * \param[in] cb    Trace callback.
 * \param[in] thunk Thunk to be passed to callback.
 */
M_API void M_state_machine_enable_trace(M_state_machine_t *m, M_state_machine_trace_cb cb, void *thunk);


/*! Enabling tracing of cleanup state machine flow.
 *
 * \param[in] m     The cleanup state machine.
 * \param[in] cb    Trace callback.
 * \param[in] thunk Thunk to be passed to callback.
 */
M_API void M_state_machine_cleanup_enable_trace(M_state_machine_cleanup_t *m, M_state_machine_trace_cb cb, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Run the state machine.
 *
 * This may need to be called multiple times. A state can run non-blocking (poll based) where
 * the state can return a wait state. The wait state means it finished processing but has
 * more to do.
 *
 * On error the cleanup state machine for the state will be called. When returning from a sub state machine
 * which had clean up run the post function which can override and ignore an error can stop the cleanup
 * process. Thus cleanup can be stopped and the state machine can recover from the error that started
 * the process.
 *
 * \param[in,out] m    The state machine to run.
 * \param[in,out] data State specific data that can be used and or manipulated by each state.
 *
 * \return Result. The done and wait returns are the only successful results (wait requiring additional calls).
 *                 All other results are error conditions.
 */
M_API M_state_machine_status_t M_state_machine_run(M_state_machine_t *m, void *data);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Rest a running state machine.
 *
 * A condition outside of the state machine could determine it needs to restart while it was
 * in a running state. Not specifically running but in the middle of a run; having returned from
 * a wait state for example. This will reset the state machine's internal process state so that
 * it can be started from the beginning again.
 *
 * This will not run cleanup immediately if requested but instead sets the state machine to start
 * cleanup on next run. The sub state machine post function will not allow overriding the cleanup
 * result and prevents the state machine from stopping cleanup. M_state_machine_run *MUST* be called.
 * Also, remember that cleanup state machines can call wait so it may be necessary to run multiple times.
 *
 * \param[in,out] m      The state machine.
 * \param[in]     reason Whether state cleanups should run. Cleanup callbacks told cleanup is due to the
 *                       reason code. Use M_STATE_MACHINE_CLEANUP_REASON_NONE to prevent cleanup.
 */
M_API void M_state_machine_reset(M_state_machine_t *m, M_state_machine_cleanup_reason_t reason);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the numeric description of the state machine
 *
 * \param[in] m       The state machine.
 * \param[in] recurse Recurs into each running sub state machine and return the description for the one running.
 *
 * \return The ndescr specified creating the state machine.
 */
M_API M_uint64 M_state_machine_ndescr(const M_state_machine_t *m, M_bool recurse);


/*! Get the description of the state machine
 *
 * \param[in] m       The state machine.
 * \param[in] recurse Recurse into each running sub state machine and return the description for the one running.
 *
 * \return The descr text specified creating the state machine.
 */
M_API const char *M_state_machine_descr(const M_state_machine_t *m, M_bool recurse);


/*! Get the active sub state machine that is currently running.
 *
 * \param[in] m       The state machine.
 * \param[in] recurse Recurse into each running sub state machine and return the last one that is running.
 *
 * \return Sub state machine if one is currently running. Otherwise, NULL.
 */
M_API const M_state_machine_t *M_state_machine_active_sub(const M_state_machine_t *m, M_bool recurse);


/*! Get state of the state machine.
 *
 * This only returns information about the given state machine. It does not look into sub state machines if one
 * is running.
 *
 * \param[in]  m  The state machine.
 * \param[out] id The id of the state currently being run. Optional pass NULL if only checking whether the
 *                state machine is running.
 *
 * \return M_TRUE if the state machine has been started and the id is a valid state id. Otherwise M_FALSE.
 */
M_API M_bool M_state_machine_active_state(const M_state_machine_t *m, M_uint64 *id);


/*! Get the numeric description for the currently running state.
 *
 * \param[in] m       The state machine.
 * \param[in] recurse Recurse into each running sub state machine and return the description for the one running.
 *
 * \return The ndescr specified when adding the state to the state machine.
 */
M_API M_uint64 M_state_machine_active_state_ndescr(const M_state_machine_t *m, M_bool recurse);


/*! Get the description text for the currently running state.
 *
 * \param[in] m       The state machine.
 * \param[in] recurse Recurse into each running sub state machine and return the description for the one running.
 *
 * \return The descr text specified when adding the state to the state machine.
 */
M_API const char *M_state_machine_active_state_descr(const M_state_machine_t *m, M_bool recurse);


/*! Get a textual representation of state machine and it's current state.
 *
 * \param[in] m       The state machine.
 * \param[in] show_id M_TRUE if the numeric representation of state ids should be included.
 *
 * \return A compound description of every machine and state.
 */
M_API char *M_state_machine_descr_full(const M_state_machine_t *m, M_bool show_id);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate an existing state machine.
 *
 * \param[in] m State machine to duplicate.
 *
 * \return New state machine.
 */
M_API M_state_machine_t *M_state_machine_duplicate(const M_state_machine_t *m) M_MALLOC;

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_STATE_MACHINE_H__ */

