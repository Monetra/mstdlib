#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_state_machine_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	STATE_A = 1,
	STATE_B,
	STATE_C,
	STATE_D,
	STATE_E,
	STATE_F,
	STATE_G,
	STATE_H,
	STATE_I
} sm_states_t;

typedef enum {
	STATE_CLEANUP_A = 1,
	STATE_CLEANUP_B,
	STATE_CLEANUP_C,
	STATE_CLEANUP_D,
	STATE_CLEANUP_E,
	STATE_CLEANUP_F
} sm_cleanup_states_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if 0
static void sm_tracer(M_state_machine_trace_t trace, M_uint64 mndescr, const char *mdescr, M_uint64 sndescr, const char *sdescr, const char *fdescr, M_uint64 id, M_state_machine_status_t status, M_bool run_sub, M_uint64 next_id, void *thunk)
{
	//const char *t = NULL;

	(void)trace;
	(void)mndescr;
	(void)mdescr;
	(void)sndescr;
	(void)sdescr;
	(void)fdescr;
	(void)id;
	(void)status;
	(void)run_sub;
	(void)next_id;
	(void)thunk;

M_printf("==== %s\n", fdescr);
#if 0
	switch (trace) {
		case M_STATE_MACHINE_TRACE_NONE:
			t = "None:    ";
			break;
		case M_STATE_MACHINE_TRACE_MACHINEENTER:
			t = "Enter:   ";
			break;
		case M_STATE_MACHINE_TRACE_MACHINEEXIT:
			t = "Exit:    ";
			break;
		case M_STATE_MACHINE_TRACE_STATE_START:
			t = "State:   ";
			break;
		case M_STATE_MACHINE_TRACE_PRE_START:
			t = "Pre:     ";
			break;
		case M_STATE_MACHINE_TRACE_POST_START:
			t = "Post:    ";
			break;
		case M_STATE_MACHINE_TRACE_CLEANUP:
			M_printf("cleanup: [M] %s -> [S] %s\n", mdescr, sdescr);
			break;
	}
	if (!M_str_isempty(t))
		M_printf("%s[M] %s -> [S] %s\n", t, mdescr, sdescr);
#endif
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_state_machine_status_t state_a(void *data, M_uint64 *next)
{
	int *d;

	(void)next;

	d = data;
	(*d)++;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_aminus(void *data, M_uint64 *next)
{
	int *d;

	(void)next;

	d = data;
	(*d)--;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_b(void *data, M_uint64 *next)
{
	int *d;

	d = data;
	if (*d < 2) {
		return M_STATE_MACHINE_STATUS_PREV;
	}

	if (*d == 192) {
		*next = STATE_D;
	} else if (*d == 300) {
		*d = 8000;
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_c(void *data, M_uint64 *next)
{
	(void)data;
	*next = STATE_B;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_d(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_DONE;
}

static M_state_machine_status_t state_e(void *data, M_uint64 *next)
{
	(void)data;
	*next = STATE_E;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_f(void *data, M_uint64 *next)
{
	int *d;

	d = data;
	
	if (*d != 2) {
		(*d) = 2;
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	*next = STATE_D;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_i(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static M_state_machine_status_t state_cleanup_a(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	int *d;

	(void)reason;
	(void)next;

	d = data;
	while (*d > 0) {
		(*d)--;
	}
	(*d)++;

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_cleanup_b(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	int *d;

	(void)reason;
	(void)next;

	d = data;
	while (*d < 100) {
		(*d)++;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_cleanup_c(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	int *d;

	(void)reason;
	(void)next;

	d = data;
	while (*d > 0) {
		(*d)--;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_cleanup_d(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	int *d;

	(void)reason;
	(void)next;

	d = data;
	*d = 4;

	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static M_state_machine_status_t state_cleanup_e(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	int *d;

	(void)reason;
	(void)next;

	d = data;
	if (*d != 9999) {
		*d = 9999;
		return M_STATE_MACHINE_STATUS_WAIT;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_cleanup_f(void *data, M_state_machine_cleanup_reason_t reason, M_uint64 *next)
{
	(void)data;
	(void)reason;
	(void)next;
	return M_STATE_MACHINE_STATUS_DONE;
}

static M_bool state_pre_to40(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	int *d = data;
	(void)status;
	if (*d > 40) {
		*next = STATE_C;
		return M_FALSE;
	} else if (*d == 40) {
		*next = STATE_D;
		return M_FALSE;
	} else if (*d < 40) {
		*next = STATE_A;
		return M_FALSE;
	}
	return M_TRUE;
}

static M_bool state_pre_40tod(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	int *d = data;
	(void)status;
	if (*d == 40) {
		*next = STATE_D;
		return M_FALSE;
	}
	return M_TRUE;
}

static M_state_machine_status_t state_post_tonext(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	int *d = data;
	(void)data;
	(void)sub_status;
	(*d)++;
	*next = STATE_E;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_post_todone(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	int *d = data;
	(void)sub_status;
	(void)next;
	(*d)++;
	return M_STATE_MACHINE_STATUS_DONE;
}

static M_state_machine_status_t state_post_forward_status(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	int *d = data;
	(void)data;
	(void)sub_status;
	(void)next;
	(*d)++;
	return sub_status;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_sm_linear)
{
	M_state_machine_t        *sm;
	M_state_machine_status_t  status;
	int                       d;

	sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_b, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, NULL, state_d, NULL, NULL);

	d = 0;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 2, "State machine did not run properly d != 2, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_nonlinear)
{
	M_state_machine_t        *sm;
	M_list_u64_t             *b_trans;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);
	b_trans = M_list_u64_create(M_LIST_U64_SORTASC);
	M_list_u64_insert(b_trans, STATE_C);
	M_list_u64_insert(b_trans, STATE_D);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_c, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, NULL, state_d, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_b, NULL, b_trans);

	d = 191;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 192, "State machine did not run properly d != 192, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_linear_no_end)
{
	M_state_machine_t        *sm;
	M_state_machine_status_t  status;
	int                       d;

	sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_b, NULL, NULL);

	d = 2;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_NO_NEXT, "State machine failure, %d", status);
	ck_assert_msg(d == 3, "State machine did not run properly d != 3, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_cleanup)
{
	M_state_machine_t         *sm;
	M_state_machine_cleanup_t *cm;
	M_state_machine_cleanup_t *cm2;
	M_state_machine_status_t   status;
	int                        d;

	cm = M_state_machine_cleanup_create(1, "CM", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_A, 1, "CU A", state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_B, 1, "CU B", state_cleanup_b, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_C, 1, "CU C", state_cleanup_c, NULL, NULL);

	cm2 = M_state_machine_cleanup_create(2, "CM2", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_D, 2, "CU D", state_cleanup_d, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_A, 2, "CU A", state_cleanup_a, NULL, NULL);

	sm = M_state_machine_create(0, "SM", M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(sm, STATE_A, 0, "SA", state_a, cm2, NULL);
	M_state_machine_insert_state(sm, STATE_B, 0, "SB", state_b, cm, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, "SC", state_d, NULL, NULL);

	d = 299;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_STATE, "State machine failure, %d", status);
	ck_assert_msg(d == 4, "State machine cleanup did not run properly d != 4, d == %d\n", d);

	M_state_machine_destroy(sm);
	M_state_machine_cleanup_destroy(cm2);
	M_state_machine_cleanup_destroy(cm);
}
END_TEST

START_TEST(check_sm_reset)
{
	M_state_machine_t         *sm;
	M_state_machine_t         *sm2;
	M_state_machine_t         *sm3;
	M_state_machine_t         *sm4;
	M_state_machine_cleanup_t *cm;
	M_state_machine_cleanup_t *cm2;
	M_state_machine_status_t   status;
	int                        d;


	/* sm3 STATE_F throws wait
 	 * reset
	 * cleanup sm3 STATE_G
	 * cleanup sm3 STATE_A
	 * cleanup sm2 STATE_B
	 * cleanup sm2 STATE_A
	 * cleanup sm1 STATE_B
	 * cleanup sm1 STATE_A
	 */

	cm = M_state_machine_cleanup_create(1, "cm", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_A, 1, NULL, state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_B, 1, NULL, state_cleanup_b, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_C, 1, NULL, state_cleanup_c, NULL, NULL);

	cm2 = M_state_machine_cleanup_create(2, "cm2", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_D, 2, NULL, state_cleanup_d, NULL, NULL);

	sm4 = M_state_machine_create(4, "sm4", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(sm4, STATE_A, 4, "STATE_A", state_a, NULL, NULL);
	M_state_machine_insert_state(sm4, STATE_B, 4, "STATE_B", state_b, NULL, NULL);
	M_state_machine_insert_state(sm4, STATE_D, 4, "STATE_D", state_d, NULL, NULL);

	M_state_machine_cleanup_insert_sub_state_machine(cm, STATE_D, 0, "CM SM4", sm4, NULL, NULL, NULL, NULL);

	sm3 = M_state_machine_create(3, "sm3", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(sm3, STATE_C, 3, "STATE_A", state_c, cm2, NULL);
	M_state_machine_insert_state(sm3, STATE_A, 3, "STATE_B", state_a, NULL, NULL);
	M_state_machine_insert_state(sm3, STATE_B, 3, "STATE_B", state_b, NULL, NULL);
	M_state_machine_insert_state(sm3, STATE_G, 3, "STATE_G", state_b, cm2, NULL);
	M_state_machine_insert_state(sm3, STATE_F, 3, "STATE_F", state_f, cm, NULL);
	M_state_machine_insert_sub_state_machine(sm3, STATE_H, 3, "STATE_C", sm4, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm3, STATE_D, 3, "STATE_D", state_d, NULL, NULL);

	sm2 = M_state_machine_create(2, "sm2", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(sm2, STATE_A, 2, "STATE_A", state_a, cm2, NULL);
	M_state_machine_insert_state(sm2, STATE_B, 2, "STATE_B", state_b, cm, NULL);
	M_state_machine_insert_sub_state_machine(sm2, STATE_C, 2, "STATE_C", sm3, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm2, STATE_D, 2, "STATE_D", state_d, NULL, NULL);

	sm = M_state_machine_create(1, "sm", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(sm, STATE_A, 1, "STATE_A", state_a, cm2, NULL);
	M_state_machine_insert_state(sm, STATE_B, 1, "STATE_B", state_b, cm, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_C, 1, "STATE_C", sm2, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 1, "STATE_D", state_d, NULL, NULL);

#if 0
	M_state_machine_enable_trace(sm, sm_tracer, NULL);
#endif

	d = 1;
	M_state_machine_run(sm, (void *)&d);
	M_state_machine_reset(sm, M_STATE_MACHINE_CLEANUP_REASON_CANCEL);
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 4, "State machine cleanup did not run properly d != 4, d == %d\n", d);

	M_state_machine_destroy(sm4);
	M_state_machine_destroy(sm3);
	M_state_machine_destroy(sm2);
	M_state_machine_destroy(sm);
	M_state_machine_cleanup_destroy(cm2);
	M_state_machine_cleanup_destroy(cm);
}
END_TEST

START_TEST(check_sm_reset_cleanup)
{
	M_state_machine_t         *sm;
	M_state_machine_cleanup_t *cm;
	M_state_machine_status_t   status;
	int                        d;

	cm = M_state_machine_cleanup_create(1, "cm", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_E, 1, "STATE_C_E", state_cleanup_e, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_A, 1, "STATE_C_A", state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_F, 1, "STATE_C_F", state_cleanup_f, NULL, NULL);

	sm = M_state_machine_create(1, "sm", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(sm, STATE_A, 1, "STATE_A", state_a, cm, NULL);
	M_state_machine_insert_state(sm, STATE_F, 1, "STATE_F", state_f, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 1, "STATE_D", state_d, NULL, NULL);

#if 0
	M_state_machine_enable_trace(sm, sm_tracer, NULL);
#endif

	d = 101;
	M_state_machine_run(sm, (void *)&d);

	/* Cancel the sm. */
	M_state_machine_reset(sm, M_STATE_MACHINE_CLEANUP_REASON_CANCEL);
	status = M_state_machine_run(sm, (void *)&d);

	/* Check we're waiting in the csm. */
	ck_assert_msg(d == 9999, "State machine cleanup did not run properly d != 9999, d == %d\n", d);

	/* Cancel the csm. */
	M_state_machine_reset(sm, M_STATE_MACHINE_CLEANUP_REASON_CANCEL);
	d = 1;

	/* Run the sm. */
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 1, "State machine cleanup did not run properly d != 1, d == %d\n", d);

	M_state_machine_destroy(sm);
	M_state_machine_cleanup_destroy(cm);
}
END_TEST

START_TEST(check_sm_descr)
{
	M_state_machine_t         *sm;
	M_state_machine_cleanup_t *cm;
	M_state_machine_cleanup_t *cm2;
	M_state_machine_cleanup_t *cm3;
	M_state_machine_status_t   status;
	const M_state_machine_t   *sub;
	const char                *descr;
	char                      *descr_m;
	const char                *fdescr = "[M] SM -> [S] SA (1) -> [CM] CM2 -> [S] CUSD (4) -> [CM] CM3 -> [S] CUSE (5)";
	M_uint64                   id;
	int                        d;

	cm3 = M_state_machine_cleanup_create(3, "CM3", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm3, STATE_CLEANUP_A, 3, "CUSA", state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm3, STATE_CLEANUP_B, 3, "CUSB", state_cleanup_b, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm3, STATE_CLEANUP_E, 3, "CUSE", state_cleanup_e, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm3, STATE_CLEANUP_F, 3, "CUSF", state_cleanup_f, NULL, NULL);

	cm2 = M_state_machine_cleanup_create(2, "CM2", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_A, 2, "CUSA", state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_D, 2, "CUSD", state_cleanup_d, cm3, NULL);
	M_state_machine_cleanup_insert_state(cm2, STATE_CLEANUP_B, 2, "CUSB", state_cleanup_b, NULL, NULL);

	cm = M_state_machine_cleanup_create(1, "CM", M_STATE_MACHINE_LINEAR_END);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_A, 1, "CUSA", state_cleanup_a, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_B, 1, "CUSB", state_cleanup_b, NULL, NULL);
	M_state_machine_cleanup_insert_state(cm, STATE_CLEANUP_C, 1, "CUSC", state_cleanup_c, NULL, NULL);

	sm = M_state_machine_create(1, "SM", M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(sm, STATE_A, 1, "SA", state_a, cm2, NULL);
	M_state_machine_insert_state(sm, STATE_B, 1, "SB", state_b, cm, NULL);
	M_state_machine_insert_state(sm, STATE_D, 1, "SC", state_d, NULL, NULL);

	d = 299;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_WAIT, "State machine failure, %d", status);

	descr = M_state_machine_descr(sm, M_FALSE);
	ck_assert_msg(M_str_eq(descr, "SM"), "State machine sm descr got: '%s', expected: 'SM'", descr);

	descr = M_state_machine_descr(sm, M_TRUE);
	ck_assert_msg(M_str_eq(descr, "CM3"), "State machine cm3 descr got: '%s', expected: 'CM3'", descr);

	descr = M_state_machine_active_state_descr(sm, M_FALSE);
	ck_assert_msg(M_str_eq(descr, "SB"), "State machine sm state b descr got: '%s', expected: 'SB'", descr);

	descr = M_state_machine_active_state_descr(sm, M_TRUE);
	ck_assert_msg(M_str_eq(descr, "CUSE"), "State machine cm3 state e descr got: '%s', expected: 'CUSE'", descr);

	descr_m = M_state_machine_descr_full(sm, M_TRUE);
	ck_assert_msg(M_str_eq(descr_m, fdescr), "State machine cm3 state e descr got: '%s', expected: '%s'", descr_m, fdescr);
	M_free(descr_m);

	ck_assert_msg(M_state_machine_active_state(sm, &id), "Could not get active state for sm");
	ck_assert_msg(id == 2, "State machine sm state got: '%llu', expected: '2'", id);

	sub = M_state_machine_active_sub(sm, M_TRUE);
	ck_assert_msg(M_state_machine_active_state(sub, &id), "Could not get active state for cm2");
	ck_assert_msg(id == 5, "State machine cm2 state got: '%llu', expected: '5'", id);

	ck_assert_msg(d == 9999, "State machine cleanup did not run properly d != 9999, d == %d\n", d);

	M_state_machine_destroy(sm);
	M_state_machine_cleanup_destroy(cm3);
	M_state_machine_cleanup_destroy(cm2);
	M_state_machine_cleanup_destroy(cm);
}
END_TEST

START_TEST(check_sm_invalid_trans)
{
	M_state_machine_t        *sm;
	M_list_u64_t             *b_trans;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);
	b_trans = M_list_u64_create(M_LIST_U64_SORTASC);
	M_list_u64_insert(b_trans, STATE_A);
	M_list_u64_insert(b_trans, STATE_C);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_c, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, NULL, state_d, NULL, NULL);
	/* B calls trans to D wich is not in it's allowed trans list. */
	M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_b, NULL, b_trans);

	d = 191;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_BAD_NEXT, "State machine failure, %d", status);
	ck_assert_msg(d == 192, "State machine did not run properly d != 192, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_self_trans)
{
	M_state_machine_t        *sm;
	M_state_machine_status_t  status;

	sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(sm, STATE_E, 0, NULL, state_e, NULL, NULL);
	status = M_state_machine_run(sm, NULL);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_SELF_NEXT, "State machine failure, %d", status);

	M_state_machine_destroy(sm);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_sm_subs)
{
	M_state_machine_t        *sm;
	M_state_machine_t        *sm_sub1;
	M_state_machine_t        *sm_sub2;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub1 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub2 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm_sub2, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_B, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_C, 0, NULL, state_a, NULL, NULL);

	M_state_machine_insert_state(sm_sub1, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_B, 0, NULL, sm_sub2, NULL, NULL, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_C, 0, NULL, sm_sub2, NULL, NULL, NULL, NULL);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_B, 0, NULL, sm_sub1, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_a, NULL, NULL);

	M_state_machine_destroy(sm_sub2);
	M_state_machine_destroy(sm_sub1);

	d = 0;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 9, "State machine did not run properly d != 9, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_error)
{
	M_state_machine_t        *sm;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_B, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_b, NULL, NULL);

	d = 298;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_STATE, "State machine failure, %d", status);
	ck_assert_msg(d == 8000, "State machine did not run properly d != 8000, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_subs_error)
{
	M_state_machine_t        *sm;
	M_state_machine_t        *sm_sub1;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub1 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm_sub1, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub1, STATE_B, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub1, STATE_C, 0, NULL, state_i, NULL, NULL);

	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_B, 0, NULL, sm_sub1, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_e, NULL, NULL);

	M_state_machine_destroy(sm_sub1);

	d = 0;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_STATE, "State machine failure, %d", status);
	ck_assert_msg(d == 3, "State machine did not run properly d != 3, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_pre)
{
	M_state_machine_t        *sm;
	M_state_machine_t        *sm_sub1;
	M_state_machine_t        *sm_sub2;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub1 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub2 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm_sub2, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_B, 0, NULL, state_aminus, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_C, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_D, 0, NULL, state_d, NULL, NULL);

	M_state_machine_insert_state(sm_sub1, STATE_A, 0, NULL, state_aminus, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_B, 0, NULL, sm_sub2, NULL, NULL, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_C, 0, NULL, sm_sub2, NULL, NULL, NULL, NULL);
	M_state_machine_insert_state(sm_sub1, STATE_D, 0, NULL, state_d, NULL, NULL);

	/* None of the subs will run due to the pres. */
	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_B, 0, NULL, sm_sub1, state_pre_to40, NULL, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_C, 0, NULL, sm_sub1, state_pre_40tod, NULL, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, NULL, state_d, NULL, NULL);

	M_state_machine_destroy(sm_sub2);
	M_state_machine_destroy(sm_sub1);

	d = 0;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_DONE, "State machine failure, %d", status);
	ck_assert_msg(d == 40, "State machine did not run properly d != 40, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

START_TEST(check_sm_post)
{
	M_state_machine_t        *sm;
	M_state_machine_t        *sm_sub1;
	M_state_machine_t        *sm_sub2;
	M_state_machine_status_t  status;
	int                       d;

	sm      = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub1 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	sm_sub2 = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm_sub2, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_B, 0, NULL, state_aminus, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_C, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_D, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_state(sm_sub2, STATE_E, 0, NULL, state_i, NULL, NULL);

	M_state_machine_insert_state(sm_sub1, STATE_A, 0, NULL, state_aminus, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_B, 0, NULL, sm_sub2, NULL, state_post_tonext, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm_sub1, STATE_C, 0, NULL, sm_sub2, NULL, state_post_todone, NULL, NULL);
	M_state_machine_insert_state(sm_sub1, STATE_D, 0, NULL, state_aminus, NULL, NULL);
	M_state_machine_insert_state(sm_sub1, STATE_E, 0, NULL, state_i, NULL, NULL);

	/* None of the subs will run due to the pres. */
	M_state_machine_insert_state(sm, STATE_A, 0, NULL, state_a, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_B, 0, NULL, sm_sub1, NULL, state_post_tonext, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_C, 0, NULL, state_d, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_E, 0, NULL, sm_sub1, NULL, state_post_forward_status, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_D, 0, NULL, state_d, NULL, NULL);

	M_state_machine_destroy(sm_sub2);
	M_state_machine_destroy(sm_sub1);

	d = 0;
	status = M_state_machine_run(sm, (void *)&d);

	ck_assert_msg(status == M_STATE_MACHINE_STATUS_ERROR_STATE, "State machine failure, %d", status);
	ck_assert_msg(d == 7, "State machine did not run properly d != 7, d == %d\n", d);

	M_state_machine_destroy(sm);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_state_machine_suite(void)
{
	Suite *suite;

	TCase *tc_sm_linear;
	TCase *tc_sm_nonlinear;
	TCase *tc_sm_linear_no_end;
	TCase *tc_sm_cleanup;
	TCase *tc_sm_reset;
	TCase *tc_sm_descr;
	TCase *tc_sm_invalid_trans;
	TCase *tc_sm_self_trans;
	TCase *tc_sm_subs;
	TCase *tc_sm_error;
	TCase *tc_sm_subs_error;
	TCase *tc_sm_pre;
	TCase *tc_sm_post;

	suite = suite_create("state_machine");

	tc_sm_linear = tcase_create("sm_linear");
	tcase_add_unchecked_fixture(tc_sm_linear, NULL, NULL);
	tcase_add_test(tc_sm_linear, check_sm_linear);
	suite_add_tcase(suite, tc_sm_linear);

	tc_sm_nonlinear = tcase_create("sm_nonlinear");
	tcase_add_unchecked_fixture(tc_sm_nonlinear, NULL, NULL);
	tcase_add_test(tc_sm_nonlinear, check_sm_nonlinear);
	suite_add_tcase(suite, tc_sm_nonlinear);

	tc_sm_linear_no_end = tcase_create("sm_linear_no_end");
	tcase_add_unchecked_fixture(tc_sm_linear_no_end, NULL, NULL);
	tcase_add_test(tc_sm_linear_no_end, check_sm_linear_no_end);
	suite_add_tcase(suite, tc_sm_linear_no_end);

	tc_sm_cleanup = tcase_create("sm_cleanup");
	tcase_add_unchecked_fixture(tc_sm_cleanup, NULL, NULL);
	tcase_add_test(tc_sm_cleanup, check_sm_cleanup);
	suite_add_tcase(suite, tc_sm_cleanup);

	tc_sm_reset = tcase_create("sm_reset");
	tcase_add_unchecked_fixture(tc_sm_reset, NULL, NULL);
	tcase_add_test(tc_sm_reset, check_sm_reset);
	suite_add_tcase(suite, tc_sm_reset);

	tc_sm_reset = tcase_create("sm_reset_cleanup");
	tcase_add_unchecked_fixture(tc_sm_reset, NULL, NULL);
	tcase_add_test(tc_sm_reset, check_sm_reset_cleanup);
	suite_add_tcase(suite, tc_sm_reset);

	tc_sm_descr = tcase_create("sm_descr");
	tcase_add_unchecked_fixture(tc_sm_descr, NULL, NULL);
	tcase_add_test(tc_sm_descr, check_sm_descr);
	suite_add_tcase(suite, tc_sm_descr);

	tc_sm_invalid_trans = tcase_create("sm_invalid_trans");
	tcase_add_unchecked_fixture(tc_sm_invalid_trans, NULL, NULL);
	tcase_add_test(tc_sm_invalid_trans, check_sm_invalid_trans);
	suite_add_tcase(suite, tc_sm_invalid_trans);

	tc_sm_self_trans = tcase_create("sm_self_trans");
	tcase_add_unchecked_fixture(tc_sm_self_trans, NULL, NULL);
	tcase_add_test(tc_sm_self_trans, check_sm_self_trans);
	suite_add_tcase(suite, tc_sm_self_trans);

	tc_sm_subs = tcase_create("sm_subs");
	tcase_add_unchecked_fixture(tc_sm_subs, NULL, NULL);
	tcase_add_test(tc_sm_subs, check_sm_subs);
	suite_add_tcase(suite, tc_sm_subs);

	tc_sm_error = tcase_create("sm_error");
	tcase_add_unchecked_fixture(tc_sm_error, NULL, NULL);
	tcase_add_test(tc_sm_error, check_sm_error);
	suite_add_tcase(suite, tc_sm_error);

	tc_sm_subs_error = tcase_create("sm_subs_error");
	tcase_add_unchecked_fixture(tc_sm_subs_error, NULL, NULL);
	tcase_add_test(tc_sm_subs_error, check_sm_subs_error);
	suite_add_tcase(suite, tc_sm_subs_error);

	tc_sm_pre = tcase_create("sm_pre");
	tcase_add_unchecked_fixture(tc_sm_pre, NULL, NULL);
	tcase_add_test(tc_sm_pre, check_sm_pre);
	suite_add_tcase(suite, tc_sm_pre);

	tc_sm_post = tcase_create("sm_post");
	tcase_add_unchecked_fixture(tc_sm_post, NULL, NULL);
	tcase_add_test(tc_sm_post, check_sm_post);
	suite_add_tcase(suite, tc_sm_post);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_state_machine_suite());
	srunner_set_log(sr, "check_state_machine.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
