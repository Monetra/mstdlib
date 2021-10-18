#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_fs_file_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define DNE_FILE "./DOES_NOT.EXIST"
#define TEST_DATA "abcdefghijklmnopqrstuvwxyz1234567890"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define DNE_FILE_OCD DNE_FILE"_ocd"
START_TEST(check_open_close_delete)
{
	M_fs_file_t  *fd;
	M_fs_error_t  res;

	/* Ensure we don't have any files left hanging around */
	(void)M_fs_delete(DNE_FILE_OCD, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);

	/* Open a file that does not exist and don't allow creation */
	res = M_fs_file_open(&fd, DNE_FILE_OCD, 0, M_FS_FILE_MODE_READ|M_FS_FILE_MODE_NOCREATE, NULL);
	ck_assert_msg(res != M_FS_ERROR_SUCCESS, "Could open file that shouldn't exist");

	/* Open a file that does not exist and create it */
	res = M_fs_file_open(&fd, DNE_FILE_OCD, M_FS_BUF_SIZE, M_FS_FILE_MODE_WRITE, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not open/create file");

	/* Close the open file */
	M_fs_file_close(fd);

	/* Check that the file does exit */
	ck_assert_msg(M_fs_perms_can_access(DNE_FILE_OCD, M_FS_PERMS_MODE_READ) == M_FS_ERROR_SUCCESS, "File not created");

	/* Delete the file */
	res = M_fs_delete(DNE_FILE_OCD, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not delete file");

	/* Check that the file was deleted */
	ck_assert_msg(M_fs_perms_can_access(DNE_FILE_OCD, M_FS_PERMS_MODE_NONE) != M_FS_ERROR_SUCCESS, "File not deleted");
}
END_TEST

#define DNE_FILE_WR DNE_FILE"_write_read"
START_TEST(check_write_read)
{
	M_fs_file_t  *fd;
	M_fs_error_t  res;
	size_t        data_len;
	size_t        rw_len;
	char          buf[64];

	data_len = M_str_len(TEST_DATA);

	/* Ensure we don't have any files left hanging around */
	(void)M_fs_delete(DNE_FILE_WR, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);

	/* Open the file for writing */
	res = M_fs_file_open(&fd, DNE_FILE_WR, 0, M_FS_FILE_MODE_WRITE, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not open/create file for writing");

	/* Write to the file */
	M_fs_file_write(fd, (const unsigned char *)TEST_DATA, data_len, &rw_len, M_FS_FILE_RW_FULLBUF);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && rw_len == data_len, "Could not write some or all all data to file, wrote: %"PRIu64" bytes", (M_uint64)rw_len);

	M_fs_file_close(fd);

	/* Read from the file */
	res = M_fs_file_open(&fd, DNE_FILE_WR, M_FS_BUF_SIZE, M_FS_FILE_MODE_READ|M_FS_FILE_MODE_NOCREATE, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not open/create file for reading");

	/* Read the data from the file */
	res = M_fs_file_read(fd, (unsigned char *)buf, sizeof(buf)-1, &rw_len, M_FS_FILE_RW_FULLBUF);
	buf[rw_len] = '\0';
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && rw_len == data_len && M_str_eq_max(TEST_DATA, buf, data_len), "Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", buf, (M_uint64)rw_len);

	/* Check that we can seek back and read part of the file */
	M_fs_file_seek(fd, 6, M_FS_FILE_SEEK_BEGIN);
	res = M_fs_file_read(fd, (unsigned char *)buf, sizeof(buf)-1, &rw_len, M_FS_FILE_RW_FULLBUF);
	buf[rw_len] = '\0';
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && rw_len == data_len-6 && M_str_eq_max(&(TEST_DATA[6]), buf, data_len-6), "Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", buf, (M_uint64)rw_len);

	M_fs_file_close(fd);

	(void)M_fs_delete(DNE_FILE_WR, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
}
END_TEST

#define DNE_FILE_WRSTR DNE_FILE"_write_read_str"
START_TEST(check_write_read_str)
{
	M_fs_error_t  res;
	size_t        data_len;
	char         *buf;

	data_len = M_str_len(TEST_DATA);

	/* Ensure we don't have any files left hanging around */
	(void)M_fs_delete(DNE_FILE_WRSTR, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);

	/* write the data */
	res = M_fs_file_write_bytes(DNE_FILE_WRSTR, (const unsigned char *)TEST_DATA, 0, 0, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not write to file");

	/* Read the data */
	res = M_fs_file_read_bytes(DNE_FILE_WRSTR, data_len+10, (unsigned char **)&buf, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && M_str_len(buf) == data_len && M_str_eq_max(TEST_DATA, buf, data_len), "Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", buf, (M_uint64)M_str_len(buf));
	M_free(buf);

	/* Append more data */
	res = M_fs_file_write_bytes(DNE_FILE_WRSTR, (const unsigned char *)TEST_DATA, 0, M_FS_FILE_MODE_APPEND, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not write to file");
	res = M_fs_file_read_bytes(DNE_FILE_WRSTR, data_len*2+10, (unsigned char **)&buf, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && M_str_len(buf) == data_len*2 && M_str_eq_max(TEST_DATA""TEST_DATA, buf, data_len*2), "Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", buf, (M_uint64)M_str_len(buf));
	M_free(buf);

	/* Overwrite data */
	res = M_fs_file_write_bytes(DNE_FILE_WRSTR, (const unsigned char *)TEST_DATA, 0, 0, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS, "Could not write to file");
	res = M_fs_file_read_bytes(DNE_FILE_WRSTR, data_len+10, (unsigned char **)&buf, NULL);
	ck_assert_msg(res == M_FS_ERROR_SUCCESS && M_str_len(buf) == data_len && M_str_eq_max(TEST_DATA, buf, data_len), "Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", buf, (M_uint64)M_str_len(buf));
	M_free(buf);

	(void)M_fs_delete(DNE_FILE_WRSTR, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
}
END_TEST

static M_fs_file_mode_t move_copy_modes[] = { M_FS_FILE_MODE_NONE, M_FS_FILE_MODE_PRESERVE_PERMS };

static void check_move_copy_int(const char *p1, const char *p2, M_bool move)
{
	M_fs_error_t   res;
	size_t         data_len;
	char          *buf;
	size_t         i;

	data_len = M_str_len(TEST_DATA);

	for (i=0; i<(sizeof(move_copy_modes)/sizeof(*move_copy_modes)); i++) {
		/* Ensure we don't have any files left hanging around */
		(void)M_fs_delete(p1, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
		(void)M_fs_delete(p2, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);

		/* write the data */
		res = M_fs_file_write_bytes(p1, (const unsigned char *)TEST_DATA, 0, 0, NULL);
		ck_assert_msg(res == M_FS_ERROR_SUCCESS, "idx=%"PRIu64": Could not write to file", (M_uint64)i);

		if (move) {
			/* Move the file */
			M_fs_move(p1, p2, move_copy_modes[i], NULL, M_FS_PROGRESS_NOEXTRA);
			/* Check the old file doens't exist */
			ck_assert_msg(M_fs_perms_can_access(p1, M_FS_PERMS_MODE_NONE) != M_FS_ERROR_SUCCESS, "idx=%"PRIu64": File not deleted", (M_uint64)i);
		} else {
			/* Copy the file */
			M_fs_copy(p1, p2, move_copy_modes[i], NULL, M_FS_PROGRESS_NOEXTRA);
			/* Check the old file exist */
			ck_assert_msg(M_fs_perms_can_access(p1, M_FS_PERMS_MODE_NONE) == M_FS_ERROR_SUCCESS, "idx=%"PRIu64": File deleted", (M_uint64)i);
		}

		/* Check the new file exists */
		ck_assert_msg(M_fs_perms_can_access(p2, M_FS_PERMS_MODE_NONE) == M_FS_ERROR_SUCCESS, "idx=%"PRIu64": File does not exist", (M_uint64)i);


		/* Read the data */
		res = M_fs_file_read_bytes(p2, data_len*2, (unsigned char **)&buf, NULL);
		ck_assert_msg(res == M_FS_ERROR_SUCCESS && M_str_len(buf) == data_len && M_str_eq_max(TEST_DATA, buf, data_len), "idx=%"PRIu64": Could not read some or all all data to file, read: '%s' %"PRIu64" bytes", (M_uint64)i, buf, (M_uint64)M_str_len(buf));
		M_free(buf);

		(void)M_fs_delete(p1, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
		(void)M_fs_delete(p2, M_FALSE, 0, M_FS_PROGRESS_NOEXTRA);
	}
}

#define DNE_FILE_M1 DNE_FILE"_move1"
#define DNE_FILE_M2 DNE_FILE"_move2"
START_TEST(check_move)
{
	check_move_copy_int(DNE_FILE_M1, DNE_FILE_M2, M_TRUE);
}
END_TEST

#define DNE_FILE_C1 DNE_FILE"_copy1"
#define DNE_FILE_C2 DNE_FILE"_copy2"
START_TEST(check_copy)
{
	check_move_copy_int(DNE_FILE_C1, DNE_FILE_C2, M_FALSE);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_fs_file_suite(void)
{
	Suite *suite;
	TCase *tc_file_open_close_delete;
	TCase *tc_file_write_read;
	TCase *tc_file_write_read_str;
	TCase *tc_file_move;
	TCase *tc_file_copy;

	suite = suite_create("file");

	tc_file_open_close_delete = tcase_create("check_open_close_delete");
	tcase_add_unchecked_fixture(tc_file_open_close_delete, NULL, NULL);
	tcase_add_test(tc_file_open_close_delete, check_open_close_delete);
	suite_add_tcase(suite, tc_file_open_close_delete);

	tc_file_write_read = tcase_create("check_write_read");
	tcase_add_unchecked_fixture(tc_file_write_read, NULL, NULL);
	tcase_add_test(tc_file_write_read, check_write_read);
	suite_add_tcase(suite, tc_file_write_read);

	tc_file_write_read_str = tcase_create("check_write_read_str");
	tcase_add_unchecked_fixture(tc_file_write_read_str, NULL, NULL);
	tcase_add_test(tc_file_write_read_str, check_write_read_str);
	suite_add_tcase(suite, tc_file_write_read_str);

	tc_file_move = tcase_create("check_move");
	tcase_add_unchecked_fixture(tc_file_move, NULL, NULL);
	tcase_add_test(tc_file_move, check_move);
	suite_add_tcase(suite, tc_file_move);

	tc_file_copy = tcase_create("check_copy");
	tcase_add_unchecked_fixture(tc_file_copy, NULL, NULL);
	tcase_add_test(tc_file_copy, check_copy);
	suite_add_tcase(suite, tc_file_copy);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_fs_file_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_file.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
