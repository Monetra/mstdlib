#include "thread_tests.c"

int main(void)
{
    SRunner *sr;
    int      nf;

    sr = srunner_create(M_thread_suite(M_THREAD_MODEL_COOP, "thread_coop"));
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_thread_coop.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    M_library_cleanup();

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
