#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_net.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef enum {
	NO_ENDPOINTS = 1,
} test_id_t;

typedef struct {
	M_bool is_success;
	test_id_t test_id;
} args_t;

static M_uint64 processing_halted_cb(M_bool no_endpoints, void *thunk)
{
	args_t *args = thunk;
	if (args->test_id == NO_ENDPOINTS) {
		args->is_success = (no_endpoints == M_TRUE);
	}
	return 0;
}

START_TEST(check_no_endpoints)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = NULL,
		.connect_fail_cb      = NULL,
		.disconnect_cb        = NULL,
		.process_fail_cb      = NULL,
		.processing_halted_cb = processing_halted_cb,
		.sent_cb              = NULL,
		.send_failed_cb       = NULL,
		.reschedule_cb        = NULL,
		.iocreate_cb          = NULL,
	};

	args_t args = {
		.is_success = M_FALSE,
		.test_id = NO_ENDPOINTS,
	};

	M_event_t *el = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t *sp = M_net_smtp_create(el, &cbs, &args);
	ck_assert_msg(M_net_smtp_resume(sp) == M_FALSE, "should fail with no endpoints");
	ck_assert_msg(args.is_success, "should trigger process_halted_cb with no endpoints");
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *smtp_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("smtp");

	tc = tcase_create("no-endpoints");
	tcase_add_test(tc, check_no_endpoints);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(smtp_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_smtp.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
