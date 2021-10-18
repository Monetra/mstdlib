#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_fs_path_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char      *path;
	const char      *result;
	M_uint32         flags;
	M_fs_system_t  system;
} path_norm_cases[] = {
	/* Unix */
	{ "./abc def/../xyz/./1 2 3/./xyr/.",          "xyz/1 2 3/xyr",      M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ "./abc def/../xyz/./1 2 3/./xyr/.",          "xyz/1 2 3/xyr",      M_FS_PATH_NORM_RESALL,   M_FS_SYSTEM_UNIX },
	{ "./abc.///../xyz//./123/./xyr/.",            "xyz/123/xyr",        M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ "../abc./..\\//xyz/\\/./123\\/./xyr/",       "../xyz/123/xyr",     M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ "./abc./../xyz/./123/./xyr/.",               "xyz/123/xyr",        M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ "////var/log/./mysql///5.1/../../mysql.log", "/var/log/mysql.log", M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_UNIX },
	{ "/var/.././/../test.txt",                    "/test.txt",          M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_UNIX },
	{ "someplace/..//.././test.txt",               "../test.txt",        M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ "/var/../",                                  "/",                  M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_UNIX },
	{ "someplace/../",                             ".",                  M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	{ ".",                                         ".",                  M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_UNIX },
	/* Windows */
	{ "C:\\\\Program Files\\zlib\\lib\\zlib1.dll",            "C:\\Program Files\\zlib\\lib\\zlib1.dll", M_FS_PATH_NORM_NONE, M_FS_SYSTEM_WINDOWS },
	{ ".\\abc.\\\\\\..\\xyz\\\\.\\123\\.\\xyr\\.",            "xyz\\123\\xyr",             M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "..\\abc.\\\\\\\\xyz\\\\\\.\\123\\\\.\\xyr\\",          "..\\abc.\\xyz\\123\\xyr",   M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "..\\abc.\\\\\\\\xyz/./123\\\\./xyr/",                  "..\\abc.\\xyz\\123\\xyr",   M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "../abc./xyz/123/xyr/",                                  "..\\abc.\\xyz\\123\\xyr",  M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ ".\\abc.\\..\\xyz\\.\\123\\.\\xyr\\.",                  "xyz\\123\\xyr",             M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "C:\\\\var\\log\\.\\mysql\\\\\\5.1\\..\\..\\mysql.log", "C:\\var\\log\\mysql.log",   M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS },
	{ "D:\\\\var\\..\\.\\\\..\\test.txt",                     "D:\\test.txt",              M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS },
	{ "someplace\\..\\\\..\\.\\test.txt",                     "..\\test.txt",              M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "C:\\\\var\\..\\",                                      "C:\\",                      M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS },
	{ "someplace\\..\\",                                      ".",                         M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ ".",                                                    ".",                         M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	/* Windows UNC */
	{ "\\\\var\\log\\.\\mysql\\\\\\5.1\\..\\..\\mysql.log",   "\\\\var\\log\\mysql.log",   M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS },
	{ "\\\\var\\..\\.\\\\..\\test.txt",                       "\\\\test.txt",              M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS },
	{ "\\\\..",                                               "\\\\",                      M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ "\\\\",                                                 "\\\\",                      M_FS_PATH_NORM_NONE,     M_FS_SYSTEM_WINDOWS },
	{ NULL, NULL, 0, 0 }
};

START_TEST(check_path_norm)
{
	char         *out;
	size_t        i;
	M_fs_error_t  ret;

	for (i=0; path_norm_cases[i].path != NULL; i++) {
		ret = M_fs_path_norm(&out, path_norm_cases[i].path, path_norm_cases[i].flags, path_norm_cases[i].system);
		ck_assert_msg(out != NULL, "%lu: %s clean failed", i, path_norm_cases[i].path);
		ck_assert_msg(ret == M_FS_ERROR_SUCCESS && M_str_eq(out, path_norm_cases[i].result), "%lu: Cleaned path: '%s', does not match expected path: '%s'", i, out, path_norm_cases[i].result);
		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char *path;
	const char *dir;
	const char *name;
} path_split_cases[] = {
	{ "this/is/p1",  "this/is", "p1"         },
	{ "this/is/p2/", "this/is", "p2"         },
	{ "this_is_p3",  NULL,      "this_is_p3" },
	{ "/bin",        "/",       "bin"        },
	{ "/",           "/",       NULL         },
	{ ".",           NULL,       "."          },
	{ NULL,          NULL,      NULL         }
};

START_TEST(check_path_split)
{
	char   *dir;
	char   *name;
	size_t  i;

	for (i=0; path_split_cases[i].path != NULL; i++) {
		dir  = M_fs_path_dirname(path_split_cases[i].path, M_FS_SYSTEM_UNIX);
		name = M_fs_path_basename(path_split_cases[i].path, M_FS_SYSTEM_UNIX);

//		ck_assert_msg(dir != NULL, "%lu: %s split failed", i, path_split_cases[i].path);
		ck_assert_msg(M_str_eq(dir, path_split_cases[i].dir) && M_str_eq(name, path_split_cases[i].name), "%lu: Split path: dir='%s', name='%s', does not match expected parts: dir='%s', name='%s'", i, dir, name, path_split_cases[i].dir, path_split_cases[i].name);

		M_free(dir);
		M_free(name);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_fs_path_suite(void)
{
	Suite *suite;
	TCase *tc_path_norm;
	TCase *tc_path_split;

	suite = suite_create("path");

	tc_path_norm = tcase_create("path_norm");
	tcase_add_unchecked_fixture(tc_path_norm, NULL, NULL);
	tcase_add_test(tc_path_norm, check_path_norm);
	suite_add_tcase(suite, tc_path_norm);

	tc_path_split = tcase_create("path_split");
	tcase_add_unchecked_fixture(tc_path_split, NULL, NULL);
	tcase_add_test(tc_path_split, check_path_split);
	suite_add_tcase(suite, tc_path_split);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_fs_path_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_path.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
