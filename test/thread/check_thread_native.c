#include "thread_tests.c"

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_thread_suite(M_THREAD_MODEL_NATIVE, "thread_native"));
	srunner_set_log(sr, "check_thread_native.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	M_library_cleanup();

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
