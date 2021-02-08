#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Configuration file with one value per key. */
#define CONF_SINGLEVALUE \
"key1=value1" "\n" \
"key2=value2" "\n"

/* Configuration file with multiple values per key. */
#define CONF_MULTIVALUE \
"key1=value1" "\n" \
"key1=value2" "\n" \
"key2=value3" "\n"

/* Configuration file with sections. */
#define CONF_SECTIONS \
"key0=value0" "\n" \
"key00=value00" "\n" \
"[Section1]" "\n" \
"key1=value1" "\n" \
"key11=value11" "\n" \
"[Section2]" "\n" \
"key2=value2" "\n" \
"key22=value22" "\n"

/* Configuration file with sections that have multi-value keys. */
#define CONF_SECTIONS_MULTI \
"key0=value0" "\n" \
"key0=value00" "\n" \
"[Section1]" "\n" \
"key1=value1" "\n" \
"key1=value11" "\n" \
"[Section2]" "\n" \
"key2=value2" "\n" \
"key2=value22" "\n"

/* Configuration file for registrations. */
#define CONF_REGISTRATIONS \
"buf_key=buf_value" "\n" \
"strdup_key=strdup_value" "\n" \
"int8_key=-8" "\n" \
"int16_key=-16" "\n" \
"int32_key=-32" "\n" \
"int64_key=-64" "\n" \
"uint8_key=8" "\n" \
"uint16_key=16" "\n" \
"uint32_key=32" "\n" \
"uint64_key=64" "\n" \
"sizet_key=128" "\n" \
"bool_key=yes" "\n" \
"custom_key=custom_value" "\n"

/* Configuration file with negative values. */
#define CONF_NEGATIVES \
"int8_key=-1" "\n" \
"int16_key=-2" "\n" \
"int32_key=-3" "\n" \
"int64_key=-4" "\n" \
"uint8_key=-5" "\n" \
"uint16_key=-6" "\n" \
"uint32_key=-7" "\n" \
"uint64_key=-8" "\n" \
"sizet_key=-9" "\n" \

/* Configuration file with values smaller than the data type can handle. */
#define CONF_UNDER_MIN_POSSIBLE \
"int8_key=-129" "\n" \
"int16_key=-32769" "\n" \
"int32_key=-2147483649" "\n" \
"uint8_key=-1" "\n" \
"uint16_key=-1" "\n" \
"uint32_key=-1" "\n" \

/* Configuration file with values larger than the data type can handle. */
#define CONF_OVER_MAX_POSSIBLE \
"int8_key=128" "\n" \
"int16_key=32768" "\n" \
"int32_key=2147483648" "\n" \
"uint8_key=256" "\n" \
"uint16_key=65536" "\n" \
"uint32_key=4294967296" "\n" \

/* Configuration file for unused keys test (single value). */
#define CONF_UNUSED_SINGLE \
"buf_key1=buf_value" "\n" \
"strdup_key1=strdup_value" "\n" \
"int8_key1=-8" "\n" \
"int16_key1=-16" "\n" \
"int32_key1=-32" "\n" \
"int64_key1=-64" "\n" \
"uint8_key1=8" "\n" \
"uint16_key1=16" "\n" \
"uint32_key1=32" "\n" \
"uint64_key1=64" "\n" \
"sizet_key1=128" "\n" \
"bool_key1=yes" "\n" \
"custom_key1=custom_value" "\n" \
\
"buf_key2=buf_value" "\n" \
"strdup_key2=strdup_value" "\n" \
"int8_key2=-8" "\n" \
"int16_key2=-16" "\n" \
"int32_key2=-32" "\n" \
"int64_key2=-64" "\n" \
"uint8_key2=8" "\n" \
"uint16_key2=16" "\n" \
"uint32_key2=32" "\n" \
"uint64_key2=64" "\n" \
"sizet_key2=128" "\n" \
"bool_key2=yes" "\n" \
"custom_key2=custom_value" "\n" \
\
"buf_key3=buf_value" "\n" \
"strdup_key3=strdup_value" "\n" \
"int8_key3=-8" "\n" \
"int16_key3=-16" "\n" \
"int32_key3=-32" "\n" \
"int64_key3=-64" "\n" \
"uint8_key3=8" "\n" \
"uint16_key3=16" "\n" \
"uint32_key3=32" "\n" \
"uint64_key3=64" "\n" \
"sizet_key3=128" "\n" \
"bool_key3=yes" "\n" \
"custom_key3=custom_value" "\n"

/* Configuration file for unused keys test (multiple value). */
#define CONF_UNUSED_MULTI \
"buf_key=buf_value" "\n" \
\
"strdup_key=strdup_value" "\n" \
"strdup_key=strdup_value" "\n" \
\
"int8_key=-8" "\n" \
"int8_key=-8" "\n" \
"int8_key=-10" "\n" \
\
"int16_key=-16" "\n" \
"int16_key=-17" "\n" \
"int16_key=-18" "\n" \
"int16_key=-19" "\n" \
\
"int32_key=-32" "\n" \
"int32_key=-33" "\n" \
"int32_key=-34" "\n" \
"int32_key=-35" "\n" \
"int32_key=-36" "\n" \
\
"int64_key=-64" "\n" \
"int64_key=-65" "\n" \
"int64_key=-66" "\n" \
"int64_key=-67" "\n" \
"int64_key=-68" "\n" \
"int64_key=-69" "\n" \
\
"uint8_key=8" "\n" \
"uint16_key=16" "\n" \
"uint32_key=32" "\n" \
"uint64_key=64" "\n" \
"sizet_key=128" "\n" \
"bool_key=yes" "\n" \
"custom_key=custom_value" "\n"


/* Create a temporary ini file at the given path with the given contents. */
static M_bool create_ini(const char *path, const char *contents)
{
	M_fs_file_t  *fd;
	M_fs_error_t  res;

	res = M_fs_file_open(&fd, path, 0, M_FS_FILE_MODE_WRITE, NULL);
	if (res != M_FS_ERROR_SUCCESS)
		return M_FALSE;

	res = M_fs_file_write(fd, (const unsigned char *)contents, M_str_len(contents), NULL, 0);
	if (res != M_FS_ERROR_SUCCESS) {
		M_fs_file_close(fd);
		return M_FALSE;
	}

	M_fs_file_close(fd);

	return M_TRUE;
}

/* Remove the temporary ini file at the given path. */
static M_bool remove_ini(const char *path)
{
	M_fs_error_t res;

	res = M_fs_delete(path, M_FALSE, NULL, 0);
	if (res != M_FS_ERROR_SUCCESS)
		return M_FALSE;

	return M_TRUE;
}

static M_bool buf_pass_cb(char *buf, size_t buf_len, const char *value, const char *default_val)
{
	(void)buf;
	(void)buf_len;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool strdup_pass_cb(char **mem, const char *value, const char *default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool int8_pass_cb(M_int8 *mem, const char *value, M_int8 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool int16_pass_cb(M_int16 *mem, const char *value, M_int16 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool int32_pass_cb(M_int32 *mem, const char *value, M_int32 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool int64_pass_cb(M_int64 *mem, const char *value, M_int64 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool uint8_pass_cb(M_uint8 *mem, const char *value, M_uint8 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool uint16_pass_cb(M_uint16 *mem, const char *value, M_uint16 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool uint32_pass_cb(M_uint32 *mem, const char *value, M_uint32 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool uint64_pass_cb(M_uint64 *mem, const char *value, M_uint64 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool sizet_pass_cb(size_t *mem, const char *value, size_t default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool bool_pass_cb(M_bool *mem, const char *value, M_bool default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_TRUE;
}

static M_bool custom_pass_cb(void *mem, const char *value)
{
	(void)mem;
	(void)value;

	return M_TRUE;
}

static M_bool buf_fail_cb(char *buf, size_t buf_len, const char *value, const char *default_val)
{
	(void)buf;
	(void)buf_len;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool strdup_fail_cb(char **mem, const char *value, const char *default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool int8_fail_cb(M_int8 *mem, const char *value, M_int8 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool int16_fail_cb(M_int16 *mem, const char *value, M_int16 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool int32_fail_cb(M_int32 *mem, const char *value, M_int32 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool int64_fail_cb(M_int64 *mem, const char *value, M_int64 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool uint8_fail_cb(M_uint8 *mem, const char *value, M_uint8 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool uint16_fail_cb(M_uint16 *mem, const char *value, M_uint16 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool uint32_fail_cb(M_uint32 *mem, const char *value, M_uint32 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool uint64_fail_cb(M_uint64 *mem, const char *value, M_uint64 default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool sizet_fail_cb(size_t *mem, const char *value, size_t default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool bool_fail_cb(M_bool *mem, const char *value, M_bool default_val)
{
	(void)mem;
	(void)value;
	(void)default_val;

	return M_FALSE;
}

static M_bool custom_fail_cb(void *mem, const char *value)
{
	(void)mem;
	(void)value;

	return M_FALSE;
}

static M_bool buf_real_cb(char *buf, size_t buf_len, const char *value, const char *default_val)
{
	(void)value;
	(void)default_val;

	M_snprintf(buf, buf_len, "buf_transform");

	return M_TRUE;
}

static M_bool strdup_real_cb(char **mem, const char *value, const char *default_val)
{
	(void)value;
	(void)default_val;

	*mem = M_strdup("strdup_transform");

	return M_TRUE;
}

static M_bool int8_real_cb(M_int8 *mem, const char *value, M_int8 default_val)
{
	(void)value;
	(void)default_val;

	*mem = -111;

	return M_TRUE;
}

static M_bool int16_real_cb(M_int16 *mem, const char *value, M_int16 default_val)
{
	(void)value;
	(void)default_val;

	*mem = -222;

	return M_TRUE;
}

static M_bool int32_real_cb(M_int32 *mem, const char *value, M_int32 default_val)
{
	(void)value;
	(void)default_val;

	*mem = -333;

	return M_TRUE;
}

static M_bool int64_real_cb(M_int64 *mem, const char *value, M_int64 default_val)
{
	(void)value;
	(void)default_val;

	*mem = -444;

	return M_TRUE;
}

static M_bool uint8_real_cb(M_uint8 *mem, const char *value, M_uint8 default_val)
{
	(void)value;
	(void)default_val;

	*mem = 111;

	return M_TRUE;
}

static M_bool uint16_real_cb(M_uint16 *mem, const char *value, M_uint16 default_val)
{
	(void)value;
	(void)default_val;

	*mem = 222;

	return M_TRUE;
}

static M_bool uint32_real_cb(M_uint32 *mem, const char *value, M_uint32 default_val)
{
	(void)value;
	(void)default_val;

	*mem = 333;

	return M_TRUE;
}

static M_bool uint64_real_cb(M_uint64 *mem, const char *value, M_uint64 default_val)
{
	(void)value;
	(void)default_val;

	*mem = 444;

	return M_TRUE;
}

static M_bool sizet_real_cb(size_t *mem, const char *value, size_t default_val)
{
	(void)value;
	(void)default_val;

	*mem = 555;

	return M_TRUE;
}

static M_bool bool_real_cb(M_bool *mem, const char *value, M_bool default_val)
{
	(void)value;
	(void)default_val;

	*mem = M_TRUE;

	return M_TRUE;
}

static M_bool custom_real_cb(void *mem, const char *value)
{
	(void)value;

	M_int64 *custom = mem;

	*custom = 999;

	return M_TRUE;
}

static M_bool buf_value_cb(char *buf, size_t buf_len, const char *value, const char *default_val)
{
	(void)default_val;

	M_str_cpy(buf, buf_len, value);

	return M_TRUE;
}

static M_bool strdup_value_cb(char **mem, const char *value, const char *default_val)
{
	(void)default_val;

	*mem = M_strdup(value);

	return M_TRUE;
}

static M_bool int8_value_cb(M_int8 *mem, const char *value, M_int8 default_val)
{
	(void)default_val;

	*mem = (M_int8)M_str_to_int32(value);

	return M_TRUE;
}

static M_bool int16_value_cb(M_int16 *mem, const char *value, M_int16 default_val)
{
	(void)default_val;

	*mem = (M_int16)M_str_to_int32(value);

	return M_TRUE;
}

static M_bool int32_value_cb(M_int32 *mem, const char *value, M_int32 default_val)
{
	(void)default_val;

	*mem = M_str_to_int32(value);

	return M_TRUE;
}

static M_bool int64_value_cb(M_int64 *mem, const char *value, M_int64 default_val)
{
	(void)default_val;

	*mem = M_str_to_int64(value);

	return M_TRUE;
}

static M_bool uint8_value_cb(M_uint8 *mem, const char *value, M_uint8 default_val)
{
	(void)default_val;

	*mem = (M_uint8)M_str_to_uint32(value);

	return M_TRUE;
}

static M_bool uint16_value_cb(M_uint16 *mem, const char *value, M_uint16 default_val)
{
	(void)default_val;

	*mem = (M_uint16)M_str_to_uint32(value);

	return M_TRUE;
}

static M_bool uint32_value_cb(M_uint32 *mem, const char *value, M_uint32 default_val)
{
	(void)default_val;

	*mem = M_str_to_uint32(value);

	return M_TRUE;
}

static M_bool uint64_value_cb(M_uint64 *mem, const char *value, M_uint64 default_val)
{
	(void)default_val;

	*mem = M_str_to_uint64(value);

	return M_TRUE;
}

static M_bool sizet_value_cb(size_t *mem, const char *value, size_t default_val)
{
	(void)default_val;

	*mem = (size_t)M_str_to_uint64(value);

	return M_TRUE;
}

static M_bool bool_value_cb(M_bool *mem, const char *value, M_bool default_val)
{
	(void)value;
	(void)default_val;

	*mem = M_str_istrue(value);

	return M_TRUE;
}

static M_bool custom_value_cb(void *mem, const char *value)
{
	char **custom = mem;

	*custom = M_strdup(value);

	return M_TRUE;
}

static M_bool buf_default_value_cb(char *buf, size_t buf_len, const char *value, const char *default_val)
{
	(void)value;

	M_str_cpy(buf, buf_len, default_val);

	return M_TRUE;
}

static M_bool strdup_default_value_cb(char **mem, const char *value, const char *default_val)
{
	(void)value;

	*mem = M_strdup(default_val);

	return M_TRUE;
}

static M_bool int8_default_value_cb(M_int8 *mem, const char *value, M_int8 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool int16_default_value_cb(M_int16 *mem, const char *value, M_int16 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool int32_default_value_cb(M_int32 *mem, const char *value, M_int32 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool int64_default_value_cb(M_int64 *mem, const char *value, M_int64 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool uint8_default_value_cb(M_uint8 *mem, const char *value, M_uint8 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool uint16_default_value_cb(M_uint16 *mem, const char *value, M_uint16 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool uint32_default_value_cb(M_uint32 *mem, const char *value, M_uint32 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool uint64_default_value_cb(M_uint64 *mem, const char *value, M_uint64 default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool sizet_default_value_cb(size_t *mem, const char *value, size_t default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool bool_default_value_cb(M_bool *mem, const char *value, M_bool default_val)
{
	(void)value;

	*mem = default_val;

	return M_TRUE;
}

static M_bool validate_buf_cb(void *data)
{
	char *buf = data;

	return M_str_eq(buf, "buf_value");
}

static M_bool validate_strdup_cb(void *data)
{
	char **str = data;

	return M_str_eq(*str, "strdup_value");
}

static M_bool validate_int8_cb(void *data)
{
	M_int8 *mem_int8 = data;

	return *mem_int8 < 0;
}

static M_bool validate_uint8_cb(void *data)
{
	M_uint8 *mem_uint8 = data;

	return *mem_uint8 > 0;
}

static M_bool validate_int16_cb(void *data)
{
	M_int16 *mem_int16 = data;

	return *mem_int16 % 42 == 0;
}

static M_bool validate_uint16_cb(void *data)
{
	M_uint16 *mem_uint16 = data;

	return *mem_uint16 + 5 == 10;
}

static M_bool validate_sizet_cb(void *data)
{
	size_t *mem_sizet = data;

	return *mem_sizet - 8 == 120;
}

static M_bool validate_bool_cb(void *data)
{
	M_bool *mem_bool = data;

	return !(*mem_bool);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_missing_path)
{
	M_conf_t *conf = NULL;
	char      errbuf[256];

	/* Check that not passing a path returns NULL. */
	conf = M_conf_create(NULL, M_FALSE, errbuf, sizeof(errbuf));
	ck_assert_msg(conf == NULL, "missing path should not be allowed");

	M_conf_destroy(conf);
}
END_TEST

START_TEST(check_missing_file)
{
	const char *filename = "./missing_conf.ini";
	M_conf_t   *conf     = NULL;
	char        errbuf[256];

	/* Check that passing a non-existent file returns NULL. */
	conf = M_conf_create(filename, M_FALSE, errbuf, sizeof(errbuf));
	ck_assert_msg(conf == NULL, "missing file was read successfully");

	M_conf_destroy(conf);
}
END_TEST

START_TEST(check_missing_errbuf)
{
	const char *filename = "./tmp_conf_check_missing_errbuf.ini";
	M_conf_t   *conf     = NULL;

	ck_assert_msg(create_ini(filename, CONF_SINGLEVALUE), "failed to create temporary config file");

	/* Check that not passing an error buffer is ok. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "not allowed to not pass an error buffer");

	M_conf_destroy(conf);
}
END_TEST

START_TEST(check_create_single_value)
{
	const char *filename = "./tmp_conf_check_create_single_value.ini";
	M_conf_t   *conf     = NULL;

	ck_assert_msg(create_ini(filename, CONF_SINGLEVALUE), "failed to create temporary config file");

	/* Check that we can create a conf with one value per key. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_create_multiple_values)
{
	const char *filename = "./tmp_conf_check_create_multiple_values.ini";
	M_conf_t   *conf     = NULL;

	ck_assert_msg(create_ini(filename, CONF_MULTIVALUE), "failed to create temporary config file");

	/* Check that we can create a conf with multiple values per key. */
	conf = M_conf_create(filename, M_TRUE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_fail_multiple_values)
{
	const char *filename = "./tmp_conf_check_fail_multiple_values.ini";
	M_conf_t   *conf     = NULL;

	ck_assert_msg(create_ini(filename, CONF_MULTIVALUE), "failed to create temporary config file");

	/* Check that a file with multiple values per key is invalid if multiple values per key is not allowed. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf == NULL, "multiple values allowed");

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_sections)
{
	const char   *filename = "./tmp_conf_check_sections.ini";
	M_conf_t     *conf     = NULL;
	M_list_str_t *sections = NULL;
	size_t        i;
	char          section[32];
	char          key[32];
	char          want_value[32];
	const char   *conf_value;

	ck_assert_msg(create_ini(filename, CONF_SECTIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that we get the correct number of sections. */
	sections = M_conf_get_sections(conf);
	ck_assert_msg(sections != NULL, "no sections found");
	ck_assert_msg(M_list_str_len(sections) == 2, "wrong number of sections (want 2, have %zu)", M_list_str_len(sections));

	/* Check that each section has the correct keys with the correct values. */
	for (i=0; i<M_list_str_len(sections); i++) {
		M_snprintf(section, sizeof(section), "Section%zu", i+1);

		M_snprintf(key, sizeof(key), "%s/key%zu", section, i+1);
		M_snprintf(want_value, sizeof(want_value), "value%zu", i+1);
		conf_value = M_conf_get_value(conf, key);
		ck_assert_msg(M_str_eq(conf_value, want_value), "wrong section key value (want %s, have %s)", want_value, conf_value);

		M_snprintf(key, sizeof(key), "%s/key%zu%zu", section, i+1, i+1);
		M_snprintf(want_value, sizeof(want_value), "value%zu%zu", i+1, i+1);
		conf_value = M_conf_get_value(conf, key);
		ck_assert_msg(M_str_eq(conf_value, want_value), "wrong section key value (want %s, have %s)", want_value, conf_value);
	}

	M_list_str_destroy(sections);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_no_sections)
{
	const char   *filename = "./tmp_conf_check_no_sections.ini";
	M_conf_t     *conf     = NULL;
	M_list_str_t *sections = NULL;

	ck_assert_msg(create_ini(filename, CONF_SINGLEVALUE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that this file has no sections. */
	sections = M_conf_get_sections(conf);
	ck_assert_msg(M_list_str_len(sections) == 0, "wrong number of sections (want 0, have %zu)", M_list_str_len(sections));

	M_list_str_destroy(sections);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_sections_no_multi)
{
	const char   *filename = "./tmp_conf_check_sections_no_multi.ini";
	M_conf_t     *conf     = NULL;
	M_list_str_t *sections = NULL;
	size_t        i;
	char          section[32];
	char          key[32];
	char          want_value[32];
	const char   *conf_value;
	M_list_str_t *values;

	ck_assert_msg(create_ini(filename, CONF_SECTIONS_MULTI), "failed to create temporary config file");

	/* Check that we can't read this file if multiple values per key is not allowed. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf == NULL, "multiple values in sections allowed");

	/* Check that we can read this file if multiple values per key is allowed. */
	conf = M_conf_create(filename, M_TRUE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that we get the correct number of sections. */
	sections = M_conf_get_sections(conf);
	ck_assert_msg(sections != NULL, "no sections found");
	ck_assert_msg(M_list_str_len(sections) == 2, "wrong number of sections (want 2, have %zu)", M_list_str_len(sections));

	/* Check that each section has the correct keys with the correct values. */
	for (i=0; i<M_list_str_len(sections); i++) {
		M_snprintf(section, sizeof(section), "Section%zu", i+1);

		M_snprintf(key, sizeof(key), "%s/key%zu", section, i+1);

		/* Check that M_conf_get_value() returns the first value for this key. */
		M_snprintf(want_value, sizeof(want_value), "value%zu", i+1);
		conf_value = M_conf_get_value(conf, key);
		ck_assert_msg(M_str_eq(conf_value, want_value), "wrong section key value (want %s, have %s)", want_value, conf_value);

		/* Check that all values are retrievable for this key. */
		values = M_conf_get_values(conf, key);
		ck_assert_msg(M_list_str_len(values) == 2, "wrong number of values for %s (have %zu, want 2)", key, M_list_str_len(values));

		/* Check that the values are correct for this key. */
		M_snprintf(want_value, sizeof(want_value), "value%zu", i+1);
		conf_value = M_list_str_at(values, 0);
		ck_assert_msg(M_str_eq(conf_value, want_value), "wrong key value 1 (want %s, have %s)", want_value, conf_value);

		M_snprintf(want_value, sizeof(want_value), "value%zu%zu", i+1, i+1);
		conf_value = M_list_str_at(values, 1);
		ck_assert_msg(M_str_eq(conf_value, want_value), "wrong key value 1 (want %s, have %s)", want_value, conf_value);

		M_list_str_destroy(values);
	}

	M_list_str_destroy(sections);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_single_value)
{
	const char   *filename = "./tmp_conf_check_single_value.ini";
	M_conf_t     *conf     = NULL;
	const char   *key;
	const char   *want_value;
	const char   *conf_value;
	M_list_str_t *values;

	ck_assert_msg(create_ini(filename, CONF_SINGLEVALUE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that the key has the correct value. */
	key        = "key1";
	want_value = "value1";
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	/* Check that the key has only one value. */
	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 1, "multiple values for %s", key);
	conf_value = M_list_str_at(values, 0);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s list value (want %s, have %s)", key, want_value, conf_value);
	M_list_str_destroy(values);

	/* Check that the key has the correct value. */
	key        = "key2";
	want_value = "value2";
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	/* Check that the key has only one value. */
	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 1, "multiple values for %s", key);
	conf_value = M_list_str_at(values, 0);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s list value (want %s, have %s)", key, want_value, conf_value);
	M_list_str_destroy(values);

	/* Check that this key doesn't exist. */
	key        = "key3";
	want_value = NULL;
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 0, "values exist for %s", key);
	M_list_str_destroy(values);

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_multiple_values)
{
	const char   *filename = "./tmp_conf_check_multiple_values.ini";
	M_conf_t     *conf     = NULL;
	const char   *key;
	const char   *want_value;
	const char   *conf_value;
	M_list_str_t *values;

	ck_assert_msg(create_ini(filename, CONF_MULTIVALUE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_TRUE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that getting the value of a key with multiple values returns the first value. */
	key        = "key1";
	want_value = "value1";
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	/* Check all the values for this key. */
	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 2, "wrong number of values for %s", key);
	want_value = "value1";
	conf_value = M_list_str_at(values, 0);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s list value (want %s, have %s)", key, want_value, conf_value);
	want_value = "value2";
	conf_value = M_list_str_at(values, 1);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s list value (want %s, have %s)", key, want_value, conf_value);
	M_list_str_destroy(values);

	/* Check that there is only one value for this key. */
	key        = "key2";
	want_value = "value3";
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 1, "multiple values for %s", key);
	conf_value = M_list_str_at(values, 0);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s list value (want %s, have %s)", key, want_value, conf_value);
	M_list_str_destroy(values);

	/* Check that this key doesn't exist. */
	key        = "key3";
	want_value = NULL;
	conf_value = M_conf_get_value(conf, key);
	ck_assert_msg(M_str_eq(conf_value, want_value), "wrong %s value (want %s, have %s)", key, want_value, conf_value);

	values     = M_conf_get_values(conf, key);
	ck_assert_msg(M_list_str_len(values) == 0, "values for %s", key);
	M_list_str_destroy(values);

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_invalid_registration)
{
	const char *filename = "./tmp_conf_check_invalid_registration.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	M_int64     mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that passing a bad conf object fails the registration. */
	ck_assert_msg(!M_conf_register_buf(NULL, "key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL), "buf registered with bad conf object");
	ck_assert_msg(!M_conf_register_strdup(NULL, "key", &mem_strdup, NULL, NULL, NULL), "strdup registered with bad conf object");
	ck_assert_msg(!M_conf_register_int8(NULL, "key", &mem_int8, 0, 0, 100, NULL), "int8 registered with bad conf object");
	ck_assert_msg(!M_conf_register_int16(NULL, "key", &mem_int16, 0, 0, 100, NULL), "int16 registered with bad conf object");
	ck_assert_msg(!M_conf_register_int32(NULL, "key", &mem_int32, 0, 0, 100, NULL), "int32 registered with bad conf object");
	ck_assert_msg(!M_conf_register_int64(NULL, "key", &mem_int64, 0, 0, 100, NULL), "int64 registered with bad conf object");
	ck_assert_msg(!M_conf_register_uint8(NULL, "key", &mem_uint8, 0, 0, 100, NULL), "uint8 registered with bad conf object");
	ck_assert_msg(!M_conf_register_uint16(NULL, "key", &mem_uint16, 0, 0, 100, NULL), "uint16 registered with bad conf object");
	ck_assert_msg(!M_conf_register_uint32(NULL, "key", &mem_uint32, 0, 0, 100, NULL), "uint32 registered with bad conf object");
	ck_assert_msg(!M_conf_register_uint64(NULL, "key", &mem_uint64, 0, 0, 100, NULL), "uint64 registered with bad conf object");
	ck_assert_msg(!M_conf_register_sizet(NULL, "key", &mem_sizet, 0, 0, 100, NULL), "sizet registered with bad conf object");
	ck_assert_msg(!M_conf_register_bool(NULL, "key", &mem_bool, M_FALSE, NULL), "bool registered with bad conf object");
	ck_assert_msg(!M_conf_register_custom(NULL, "key", &mem_custom, custom_pass_cb), "custom registered with bad conf object");

	/* Check that not passing a key fails the registration. */
	ck_assert_msg(!M_conf_register_buf(conf, NULL, mem_buf, sizeof(mem_buf), NULL, NULL, NULL), "buf registered without key");
	ck_assert_msg(!M_conf_register_strdup(conf, NULL, &mem_strdup, NULL, NULL, NULL), "strdup registered without key");
	ck_assert_msg(!M_conf_register_int8(conf, NULL, &mem_int8, 0, 0, 100, NULL), "int8 registered without key");
	ck_assert_msg(!M_conf_register_int16(conf, NULL, &mem_int16, 0, 0, 100, NULL), "int16 registered without key");
	ck_assert_msg(!M_conf_register_int32(conf, NULL, &mem_int32, 0, 0, 100, NULL), "int32 registered without key");
	ck_assert_msg(!M_conf_register_int64(conf, NULL, &mem_int64, 0, 0, 100, NULL), "int64 registered without key");
	ck_assert_msg(!M_conf_register_uint8(conf, NULL, &mem_uint8, 0, 0, 100, NULL), "uint8 registered without key");
	ck_assert_msg(!M_conf_register_uint16(conf, NULL, &mem_uint16, 0, 0, 100, NULL), "uint16 registered without key");
	ck_assert_msg(!M_conf_register_uint32(conf, NULL, &mem_uint32, 0, 0, 100, NULL), "uint32 registered without key");
	ck_assert_msg(!M_conf_register_uint64(conf, NULL, &mem_uint64, 0, 0, 100, NULL), "uint64 registered without key");
	ck_assert_msg(!M_conf_register_sizet(conf, NULL, &mem_sizet, 0, 0, 100, NULL), "sizet registered without key");
	ck_assert_msg(!M_conf_register_bool(conf, NULL, &mem_bool, M_FALSE, NULL), "bool registered without key");
	ck_assert_msg(!M_conf_register_custom(conf, NULL, &mem_custom, custom_pass_cb), "custom registered without key");

	/* Check that not passing an address fails the registration. */
	ck_assert_msg(!M_conf_register_buf(conf, "key", NULL, sizeof(mem_buf), NULL, NULL, NULL), "buf registered without address");
	ck_assert_msg(!M_conf_register_strdup(conf, "key", NULL, NULL, NULL, NULL), "strdup registered without address");
	ck_assert_msg(!M_conf_register_int8(conf, "key", NULL, 0, 0, 100, NULL), "int8 registered without address");
	ck_assert_msg(!M_conf_register_int16(conf, "key", NULL, 0, 0, 100, NULL), "int16 registered without address");
	ck_assert_msg(!M_conf_register_int32(conf, "key", NULL, 0, 0, 100, NULL), "int32 registered without address");
	ck_assert_msg(!M_conf_register_int64(conf, "key", NULL, 0, 0, 100, NULL), "int64 registered without address");
	ck_assert_msg(!M_conf_register_uint8(conf, "key", NULL, 0, 0, 100, NULL), "uint8 registered without address");
	ck_assert_msg(!M_conf_register_uint16(conf, "key", NULL, 0, 0, 100, NULL), "uint16 registered without address");
	ck_assert_msg(!M_conf_register_uint32(conf, "key", NULL, 0, 0, 100, NULL), "uint32 registered without address");
	ck_assert_msg(!M_conf_register_uint64(conf, "key", NULL, 0, 0, 100, NULL), "uint64 registered without address");
	ck_assert_msg(!M_conf_register_sizet(conf, "key", NULL, 0, 0, 100, NULL), "sizet registered without address");
	ck_assert_msg(!M_conf_register_bool(conf, "key", NULL, M_FALSE, NULL), "bool registered without address");

	/* Check that not passing a length fails a buffer registration. */
	ck_assert_msg(!M_conf_register_buf(conf, "key", mem_buf, 0, NULL, NULL, NULL), "buf registered without length");

	/* Check that not passing a callback fails a custom registration. */
	ck_assert_msg(!M_conf_register_custom(conf, "key", &mem_custom, NULL), "custom registered without callback");

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_registration_args)
{
	const char *filename = "./tmp_conf_check_registration_args.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	M_int64     mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that we can register a key with no validation. */
	ck_assert_msg(M_conf_register_buf(conf, "key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL), "buf not registered without validation");
	ck_assert_msg(M_conf_register_strdup(conf, "key", &mem_strdup, NULL, NULL, NULL), "strdup not registered without validation");
	ck_assert_msg(M_conf_register_int8(conf, "key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL), "int8 not registered without validation");
	ck_assert_msg(M_conf_register_int16(conf, "key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL), "int16 not registered without validation");
	ck_assert_msg(M_conf_register_int32(conf, "key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL), "int32 not registered without validation");
	ck_assert_msg(M_conf_register_int64(conf, "key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL), "int64 not registered without validation");
	ck_assert_msg(M_conf_register_uint8(conf, "key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL), "uint8 not registered without validation");
	ck_assert_msg(M_conf_register_uint16(conf, "key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL), "uint16 not registered without validation");
	ck_assert_msg(M_conf_register_uint32(conf, "key", &mem_uint32, 0, 0, M_UINT32_MAX, NULL), "uint32 not registered without validation");
	ck_assert_msg(M_conf_register_uint64(conf, "key", &mem_uint64, 0, 0, M_UINT64_MAX, NULL), "uint64 not registered without validation");
	ck_assert_msg(M_conf_register_sizet(conf, "key", &mem_sizet, 0, 0, SIZE_MAX, NULL), "sizet not registered without validation");
	ck_assert_msg(M_conf_register_bool(conf, "key", &mem_bool, M_FALSE, NULL), "bool not registered without validation");

	/* Check that we can register a key with a default value. */
	ck_assert_msg(M_conf_register_buf(conf, "key", mem_buf, sizeof(mem_buf), "default", NULL, NULL), "buf not registered with default value");
	ck_assert_msg(M_conf_register_strdup(conf, "key", &mem_strdup, "default", NULL, NULL), "strdup not registered with default value");
	ck_assert_msg(M_conf_register_int8(conf, "key", &mem_int8, 100, M_INT8_MIN, M_INT8_MAX, NULL), "int8 not registered with default value");
	ck_assert_msg(M_conf_register_int16(conf, "key", &mem_int16, 100, M_INT16_MIN, M_INT16_MAX, NULL), "int16 not registered with default value");
	ck_assert_msg(M_conf_register_int32(conf, "key", &mem_int32, 100, M_INT32_MIN, M_INT32_MAX, NULL), "int32 not registered with default value");
	ck_assert_msg(M_conf_register_int64(conf, "key", &mem_int64, 100, M_INT64_MIN, M_INT64_MAX, NULL), "int64 not registered with default value");
	ck_assert_msg(M_conf_register_uint8(conf, "key", &mem_uint8, 100, 0, M_UINT8_MAX, NULL), "uint8 not registered with default value");
	ck_assert_msg(M_conf_register_uint16(conf, "key", &mem_uint16, 100, 0, M_UINT16_MAX, NULL), "uint16 not registered with default value");
	ck_assert_msg(M_conf_register_uint32(conf, "key", &mem_uint32, 100, 0, M_UINT32_MAX, NULL), "uint32 not registered with default value");
	ck_assert_msg(M_conf_register_uint64(conf, "key", &mem_uint64, 100, 0, M_UINT64_MAX, NULL), "uint64 not registered with default value");
	ck_assert_msg(M_conf_register_sizet(conf, "key", &mem_sizet, 100, 0, SIZE_MAX, NULL), "sizet not registered with default value");
	ck_assert_msg(M_conf_register_bool(conf, "key", &mem_bool, M_TRUE, NULL), "bool not registered with default value");

	/* Check that we can register a key with validation. */
	ck_assert_msg(M_conf_register_buf(conf, "key", mem_buf, sizeof(mem_buf), NULL, "abc*", NULL), "buf not registered with validation");
	ck_assert_msg(M_conf_register_strdup(conf, "key", &mem_strdup, NULL, "abc*", NULL), "strdup not registered with validation");
	ck_assert_msg(M_conf_register_int8(conf, "key", &mem_int8, 0, -100, 100, NULL), "int8 not registered with validation");
	ck_assert_msg(M_conf_register_int16(conf, "key", &mem_int16, 0, -100, 100, NULL), "int16 not registered with validation");
	ck_assert_msg(M_conf_register_int32(conf, "key", &mem_int32, 0, -100, 100, NULL), "int32 not registered with validation");
	ck_assert_msg(M_conf_register_int64(conf, "key", &mem_int64, 0, -100, 100, NULL), "int64 not registered with validation");
	ck_assert_msg(M_conf_register_uint8(conf, "key", &mem_uint8, 0, 100, 200, NULL), "uint8 not registered with validation");
	ck_assert_msg(M_conf_register_uint16(conf, "key", &mem_uint16, 0, 100, 200, NULL), "uint16 not registered with validation");
	ck_assert_msg(M_conf_register_uint32(conf, "key", &mem_uint32, 0, 100, 200, NULL), "uint32 not registered with validation");
	ck_assert_msg(M_conf_register_uint64(conf, "key", &mem_uint64, 0, 100, 200, NULL), "uint64 not registered with validation");
	ck_assert_msg(M_conf_register_sizet(conf, "key", &mem_sizet, 0, 100, 200, NULL), "sizet not registered with validation");

	/* Check that we can register a key with a conversion callback. */
	ck_assert_msg(M_conf_register_buf(conf, "key", mem_buf, sizeof(mem_buf), NULL, NULL, buf_pass_cb), "buf not registered with conversion callback");
	ck_assert_msg(M_conf_register_strdup(conf, "key", &mem_strdup, NULL, NULL, strdup_pass_cb), "strdup not registered with conversion callback");
	ck_assert_msg(M_conf_register_int8(conf, "key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, int8_pass_cb), "int8 not registered with conversion callback");
	ck_assert_msg(M_conf_register_int16(conf, "key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, int16_pass_cb), "int16 not registered with conversion callback");
	ck_assert_msg(M_conf_register_int32(conf, "key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, int32_pass_cb), "int32 not registered with conversion callback");
	ck_assert_msg(M_conf_register_int64(conf, "key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, int64_pass_cb), "int64 not registered with conversion callback");
	ck_assert_msg(M_conf_register_uint8(conf, "key", &mem_uint8, 0, 0, M_UINT8_MAX, uint8_pass_cb), "uint8 not registered with conversion callback");
	ck_assert_msg(M_conf_register_uint16(conf, "key", &mem_uint16, 0, 0, M_UINT16_MAX, uint16_pass_cb), "uint16 not registered with conversion callback");
	ck_assert_msg(M_conf_register_uint32(conf, "key", &mem_uint32, 0, 0, M_UINT32_MAX, uint32_pass_cb), "uint32 not registered with conversion callback");
	ck_assert_msg(M_conf_register_uint64(conf, "key", &mem_uint64, 0, 0, M_UINT64_MAX, uint64_pass_cb), "uint64 not registered with conversion callback");
	ck_assert_msg(M_conf_register_sizet(conf, "key", &mem_sizet, 0, 0, SIZE_MAX, sizet_pass_cb), "sizet not registered with conversion callback");
	ck_assert_msg(M_conf_register_bool(conf, "key", &mem_bool, M_FALSE, bool_pass_cb), "bool not registered with conversion callback");
	ck_assert_msg(M_conf_register_custom(conf, "key", &mem_custom, custom_pass_cb), "custom not registered with conversion callback");

	/* Custom registrations should work both with and without an address. */
	ck_assert_msg(M_conf_register_custom(conf, "key", &mem_custom, custom_pass_cb), "custom not registered with address");
	ck_assert_msg(M_conf_register_custom(conf, "key", NULL, custom_pass_cb), "custom not registered without address");

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_straight_registration)
{
	const char *filename = "./tmp_conf_check_straight_registration.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that the registration reads the correct conf value. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, NULL);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed for reading");

	ck_assert_msg(M_str_eq(mem_buf, "buf_value"), "buf failed to get conf value");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup failed to get conf value");
	ck_assert_msg(mem_int8 == -8, "int8 failed to get conf value");
	ck_assert_msg(mem_int16 == -16, "int16 failed to get conf value");
	ck_assert_msg(mem_int32 == -32, "int32 failed to get conf value");
	ck_assert_msg(mem_int64 == -64, "int64 failed to get conf value");
	ck_assert_msg(mem_uint8 == 8, "uint8 failed to get conf value");
	ck_assert_msg(mem_uint16 == 16, "uint16 failed to get conf value");
	ck_assert_msg(mem_uint32 == 32, "uint32 failed to get conf value");
	ck_assert_msg(mem_uint64 == 64, "uint64 failed to get conf value");
	ck_assert_msg(mem_sizet == 128, "sizet failed to get conf value");
	ck_assert_msg(mem_bool == M_TRUE, "bool failed to get conf value");

	M_free(mem_strdup);

	/* Check that the registration uses the correct default value. */
	M_conf_register_buf(conf, "NOKEY", mem_buf, sizeof(mem_buf), "buf_default", NULL, NULL);
	M_conf_register_strdup(conf, "NOKEY", &mem_strdup, "str_default", NULL, NULL);
	M_conf_register_int8(conf, "NOKEY", &mem_int8, -99, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "NOKEY", &mem_int16, -999, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "NOKEY", &mem_int32, -9999, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "NOKEY", &mem_int64, -99999, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "NOKEY", &mem_uint8, 99, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "NOKEY", &mem_uint16, 999, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "NOKEY", &mem_uint32, 9999, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "NOKEY", &mem_uint64, 99999, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "NOKEY", &mem_sizet, 999999, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "NOKEY", &mem_bool, M_TRUE, NULL);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed for defaults");

	ck_assert_msg(M_str_eq(mem_buf, "buf_default"), "buf failed to use default value");
	ck_assert_msg(M_str_eq(mem_strdup, "str_default"), "strdup failed to use default value");
	ck_assert_msg(mem_int8 == -99, "int8 failed to use default value");
	ck_assert_msg(mem_int16 == -999, "int16 failed to use default value");
	ck_assert_msg(mem_int32 == -9999, "int32 failed to use default value");
	ck_assert_msg(mem_int64 == -99999, "int64 failed to use default value");
	ck_assert_msg(mem_uint8 == 99, "uint8 failed to use default value");
	ck_assert_msg(mem_uint16 == 999, "uint16 failed to use default value");
	ck_assert_msg(mem_uint32 == 9999, "uint32 failed to use default value");
	ck_assert_msg(mem_uint64 == 99999, "uint64 failed to use default value");
	ck_assert_msg(mem_sizet == 999999, "sizet failed to use default value");
	ck_assert_msg(mem_bool == M_TRUE, "bool failed to use default value");

	M_free(mem_strdup);

	/* Check that memory is being correctly blanked. */
	M_conf_register_buf(conf, "NOKEY", mem_buf, sizeof(mem_buf), "buf_default", NULL, buf_pass_cb);
	M_conf_register_strdup(conf, "NOKEY", &mem_strdup, "str_default", NULL, strdup_pass_cb);
	M_conf_register_int8(conf, "NOKEY", &mem_int8, -99, M_INT8_MIN, M_INT8_MAX, int8_pass_cb);
	M_conf_register_int16(conf, "NOKEY", &mem_int16, -999, M_INT16_MIN, M_INT16_MAX, int16_pass_cb);
	M_conf_register_int32(conf, "NOKEY", &mem_int32, -9999, M_INT32_MIN, M_INT32_MAX, int32_pass_cb);
	M_conf_register_int64(conf, "NOKEY", &mem_int64, -99999, M_INT64_MIN, M_INT64_MAX, int64_pass_cb);
	M_conf_register_uint8(conf, "NOKEY", &mem_uint8, 99, 0, M_UINT8_MAX, uint8_pass_cb);
	M_conf_register_uint16(conf, "NOKEY", &mem_uint16, 999, 0, M_UINT16_MAX, uint16_pass_cb);
	M_conf_register_uint32(conf, "NOKEY", &mem_uint32, 9999, 0, M_UINT32_MAX, uint32_pass_cb);
	M_conf_register_uint64(conf, "NOKEY", &mem_uint64, 99999, 0, M_UINT64_MAX, uint64_pass_cb);
	M_conf_register_sizet(conf, "NOKEY", &mem_sizet, 999999, 0, SIZE_MAX, sizet_pass_cb);
	M_conf_register_bool(conf, "NOKEY", &mem_bool, M_TRUE, bool_pass_cb);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed for blanks");

	ck_assert_msg(M_str_isempty(mem_buf), "buf was not zeroed out");
	ck_assert_msg(M_str_isempty(mem_strdup), "strdup was not zeroed out");
	ck_assert_msg(mem_int8 == 0, "int8 was not zeroed out");
	ck_assert_msg(mem_int16 == 0, "int16 was not zeroed out");
	ck_assert_msg(mem_int32 == 0, "int32 was not zeroed out");
	ck_assert_msg(mem_int64 == 0, "int64 was not zeroed out");
	ck_assert_msg(mem_uint8 == 0, "uint8 was not zeroed out");
	ck_assert_msg(mem_uint16 == 0, "uint16 was not zeroed out");
	ck_assert_msg(mem_uint32 == 0, "uint32 was not zeroed out");
	ck_assert_msg(mem_uint64 == 0, "uint64 was not zeroed out");
	ck_assert_msg(mem_sizet == 0, "sizet was not zeroed out");
	ck_assert_msg(mem_bool == M_FALSE, "bool was not zeroed out");

	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_sanity)
{
	/* Check that each registration fails its validation and sets the default value (not the zero value). */
	const char *filename = "./tmp_conf_check_sanity.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "default", "[:digit:]+", NULL);
	ck_assert_msg(!M_conf_parse(conf), "buf passed validation");
	ck_assert_msg(M_str_eq(mem_buf, "default"), "buf was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "default", "(A|B)+", NULL);
	ck_assert_msg(!M_conf_parse(conf), "strdup passed validation");
	ck_assert_msg(M_str_eq(mem_strdup, "default"), "strdup  was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 10, -2, -4, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int8 passed validation");
	ck_assert_msg(mem_int8 == 10, "int8 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 10, -2, -4, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int16 passed validation");
	ck_assert_msg(mem_int16 == 10, "int16 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 10, -2, -4, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int32 passed validation");
	ck_assert_msg(mem_int32 == 10, "int32 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 10, -2, -4, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int64 passed validation");
	ck_assert_msg(mem_int64 == 10, "int64 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 10, 4, 6, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint8 passed validation");
	ck_assert_msg(mem_uint8 == 10, "uint8 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 10, 4, 6, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint16 passed validation");
	ck_assert_msg(mem_uint16 == 10, "uint16 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 10, 4, 6, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint32 passed validation");
	ck_assert_msg(mem_uint32 == 10, "uint32 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 10, 4, 6, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint64 passed validation");
	ck_assert_msg(mem_uint64 == 10, "uint64 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 10, 4, 6, NULL);
	ck_assert_msg(!M_conf_parse(conf), "sizet passed validation");
	ck_assert_msg(mem_sizet == 10, "sizet was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_negatives)
{
	/* Check that negative values are or are not allowed integer registrations depending on the data type. */
	const char *filename = "./tmp_conf_check_negatives.ini";
	M_conf_t   *conf     = NULL;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;

	ck_assert_msg(create_ini(filename, CONF_NEGATIVES), "failed to create temporary config file");

	/* Signed integer registrations should allow negative values. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	ck_assert_msg(M_conf_parse(conf), "int8 not allowed to have a negative value");
	ck_assert_msg(mem_int8 == -1, "int8 has wrong value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	ck_assert_msg(M_conf_parse(conf), "int16 not allowed to have a negative value");
	ck_assert_msg(mem_int16 == -2, "int16 has wrong value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	ck_assert_msg(M_conf_parse(conf), "int32 not allowed to have a negative value");
	ck_assert_msg(mem_int32 == -3, "int32 has wrong value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	ck_assert_msg(M_conf_parse(conf), "int64 not allowed to have a negative value");
	ck_assert_msg(mem_int64 == -4, "int64 has wrong value");
	M_conf_destroy(conf); conf = NULL;

	/* Unsigned integer registrations should not allow negative values. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 1, 0, M_UINT8_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint8 allowed to have a negative value");
	ck_assert_msg(mem_uint8 == 1, "uint8 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 1, 0, M_UINT16_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint16 allowed to have a negative value");
	ck_assert_msg(mem_uint16 == 1, "uint16 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 1, 0, M_UINT32_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint32 allowed to have a negative value");
	ck_assert_msg(mem_uint32 == 1, "uint32 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 1, 0, M_UINT64_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint64 allowed to have a negative value");
	ck_assert_msg(mem_uint64 == 1, "uint64 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 1, 0, SIZE_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "sizet allowed to have a negative value");
	ck_assert_msg(mem_sizet == 1, "sizet was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_under_min_possible)
{
	/* Check that values below each data type's minimum possible value are not allowed. */
	const char *filename = "./tmp_conf_check_under_min_possible.ini";
	M_conf_t   *conf     = NULL;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;

	ck_assert_msg(create_ini(filename, CONF_UNDER_MIN_POSSIBLE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 2, M_INT8_MIN, M_INT8_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int8 allowed to have value below what type allows");
	ck_assert_msg(mem_int8 == 2, "int8 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 2, M_INT16_MIN, M_INT16_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int16 allowed to have value below what type allows");
	ck_assert_msg(mem_int16 == 2, "int16 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 2, M_INT32_MIN, M_INT32_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int32 allowed to have value below what type allows");
	ck_assert_msg(mem_int32 == 2, "int32 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 2, 0, M_UINT8_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint8 allowed to have value below what type allows");
	ck_assert_msg(mem_uint8 == 2, "uint8 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 2, 0, M_UINT16_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint16 allowed to have value below what type allows");
	ck_assert_msg(mem_uint16 == 2, "uint16 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 2, 0, M_UINT32_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint32 allowed to have value below what type allows");
	ck_assert_msg(mem_uint32 == 2, "uint32 was was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_over_max_possible)
{
	/* Check that values above each data type's maximum possible value are not allowed. */
	const char *filename = "./tmp_conf_check_over_max_possible.ini";
	M_conf_t   *conf     = NULL;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;

	ck_assert_msg(create_ini(filename, CONF_OVER_MAX_POSSIBLE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 3, M_INT8_MIN, M_INT8_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int8 allowed to have value above what type allows");
	ck_assert_msg(mem_int8 == 3, "int8 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 3, M_INT16_MIN, M_INT16_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int16 allowed to have value above what type allows");
	ck_assert_msg(mem_int16 == 3, "int16 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 3, M_INT32_MIN, M_INT32_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "int32 allowed to have value above what type allows");
	ck_assert_msg(mem_int32 == 3, "int32 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 3, 0, M_UINT8_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint8 allowed to have value above what type allows");
	ck_assert_msg(mem_uint8 == 3, "uint8 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 3, 0, M_UINT16_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint16 allowed to have value above what type allows");
	ck_assert_msg(mem_uint16 == 3, "uint16 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 3, 0, M_UINT32_MAX, NULL);
	ck_assert_msg(!M_conf_parse(conf), "uint32 allowed to have value above what type allows");
	ck_assert_msg(mem_uint32 == 3, "uint32 was not set to default value");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_transformation_error)
{
	/* Check that a transformation error causes M_conf_parse() to fail and the memory is set to the zero value. */
	const char *filename = "./tmp_conf_check_transformation_error.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	M_int64     mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "default", NULL, buf_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "buf passed bad transformation callback");
	ck_assert_msg(M_str_isempty(mem_buf), "buf was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "default", NULL, strdup_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "strdup passed bad transformation callback");
	ck_assert_msg(M_str_isempty(mem_strdup), "strdup was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 10, 0, 0, int8_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "int8 passed bad transformation callback");
	ck_assert_msg(mem_int8 == 0, "int8 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 10, 0, 0, int16_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "int16 passed bad transformation callback");
	ck_assert_msg(mem_int16 == 0, "int16 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 10, 0, 0, int32_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "int32 passed bad transformation callback");
	ck_assert_msg(mem_int32 == 0, "int32 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 10, 0, 0, int64_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "int64 passed bad transformation callback");
	ck_assert_msg(mem_int64 == 0, "int64 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 10, 0, 0, uint8_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "uint8 passed bad transformation callback");
	ck_assert_msg(mem_uint8 == 0, "uint8 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 10, 0, 0, uint16_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "uint16 passed bad transformation callback");
	ck_assert_msg(mem_uint16 == 0, "uint16 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 10, 0, 0, uint32_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "uint32 passed bad transformation callback");
	ck_assert_msg(mem_uint32 == 0, "uint32 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 10, 0, 0, uint64_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "uint64 passed bad transformation callback");
	ck_assert_msg(mem_uint64 == 0, "uint64 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 10, 0, 0, sizet_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "sizet passed bad transformation callback");
	ck_assert_msg(mem_sizet == 0, "sizet was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_TRUE, bool_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "bool passed bad transformation callback");
	ck_assert_msg(mem_bool == 0, "bool was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_custom(conf, "custom_key", &mem_custom, custom_fail_cb);
	ck_assert_msg(!M_conf_parse(conf), "custom passed bad transformation callback");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_transformation_override)
{
	/* Check that setting a transformation callback with the registration overrides any other validation. */
	const char *filename = "./tmp_conf_check_transformation_override.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "default", "[:digit:]+", buf_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "buf failed validation when transformation callback should be handling that");
	ck_assert_msg(M_str_isempty(mem_buf), "buf was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "default", "(A|B)+", strdup_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "strdup failed validation when transformation callback should be handling that");
	ck_assert_msg(M_str_isempty(mem_strdup), "strdup was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 10, -2, -4, int8_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "int8 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_int8 == 0, "int8 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 10, -2, -4, int16_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "int16 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_int16 == 0, "int16 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 10, -2, -4, int32_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "int32 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_int32 == 0, "int32 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 10, -2, -4, int64_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "int64 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_int64 == 0, "int64 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 10, 4, 6, uint8_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "uint8 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_uint8 == 0, "uint8 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 10, 4, 6, uint16_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "uint16 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_uint16 == 0, "uint16 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 10, 4, 6, uint32_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "uint32 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_uint32 == 0, "uint32 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 10, 4, 6, uint64_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "uint64 failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_uint64 == 0, "uint64 was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 10, 4, 6, sizet_pass_cb);
	ck_assert_msg(M_conf_parse(conf), "sizet failed validation when transformation callback should be handling that");
	ck_assert_msg(mem_sizet == 0, "sizet was not zeroed out");
	M_conf_destroy(conf); conf = NULL;

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_transformation_set)
{
	const char *filename = "./tmp_conf_check_transformation_set.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	M_int64     mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that the value set in the transformation callback is honored. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "default", NULL, buf_real_cb);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "default", NULL, strdup_real_cb);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 10, 0, 0, int8_real_cb);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 10, 0, 0, int16_real_cb);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 10, 0, 0, int32_real_cb);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 10, 0, 0, int64_real_cb);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 10, 0, 0, uint8_real_cb);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 10, 0, 0, uint16_real_cb);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 10, 0, 0, uint32_real_cb);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 10, 0, 0, uint64_real_cb);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 10, 0, 0, sizet_real_cb);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, bool_real_cb);
	M_conf_register_custom(conf, "custom_key", &mem_custom, custom_real_cb);

	ck_assert_msg(M_conf_parse(conf), "transformation callbacks failed transformation");

	ck_assert_msg(M_str_eq(mem_buf, "buf_transform"), "buf transformation failed");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_transform"), "strdup transformation failed");
	ck_assert_msg(mem_int8 == -111, "int8 transformation failed");
	ck_assert_msg(mem_int16 == -222, "int16 transformation failed");
	ck_assert_msg(mem_int32 == -333, "int32 transformation failed");
	ck_assert_msg(mem_int64 == -444, "int64 transformation failed");
	ck_assert_msg(mem_uint8 == 111, "uint8 transformation failed");
	ck_assert_msg(mem_uint16 == 222, "uint16 transformation failed");
	ck_assert_msg(mem_uint32 == 333, "uint32 transformation failed");
	ck_assert_msg(mem_uint64 == 444, "uint64 transformation failed");
	ck_assert_msg(mem_sizet == 555, "sizet transformation failed");
	ck_assert_msg(mem_custom == 999, "custom transformation failed");

	M_free(mem_strdup);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_transformation_value)
{
	const char *filename = "./tmp_conf_check_transformation_value.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	char       *mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that the correct value is sent to the transformation callback. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "default", NULL, buf_value_cb);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "default", NULL, strdup_value_cb);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 10, 0, 0, int8_value_cb);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 10, 0, 0, int16_value_cb);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 10, 0, 0, int32_value_cb);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 10, 0, 0, int64_value_cb);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 10, 0, 0, uint8_value_cb);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 10, 0, 0, uint16_value_cb);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 10, 0, 0, uint32_value_cb);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 10, 0, 0, uint64_value_cb);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 10, 0, 0, sizet_value_cb);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, bool_value_cb);
	M_conf_register_custom(conf, "custom_key", &mem_custom, custom_value_cb);

	ck_assert_msg(M_conf_parse(conf), "transformation callbacks failed transformation");

	ck_assert_msg(M_str_eq(mem_buf, "buf_value"), "buf transformation callback was sent wrong value");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup transformation callback was sent wrong value");
	ck_assert_msg(mem_int8 == -8, "int8 transformation callback was sent wrong value");
	ck_assert_msg(mem_int16 == -16, "int16 transformation callback was sent wrong value");
	ck_assert_msg(mem_int32 == -32, "int32 transformation callback was sent wrong value");
	ck_assert_msg(mem_int64 == -64, "int64 transformation callback was sent wrong value");
	ck_assert_msg(mem_uint8 == 8, "uint8 transformation callback was sent wrong value");
	ck_assert_msg(mem_uint16 == 16, "uint16 transformation callback was sent wrong value");
	ck_assert_msg(mem_uint32 == 32, "uint32 transformation callback was sent wrong value");
	ck_assert_msg(mem_uint64 == 64, "uint64 transformation callback was sent wrong value");
	ck_assert_msg(mem_sizet == 128, "sizet transformation callback was sent wrong value");
	ck_assert_msg(mem_bool == M_TRUE, "bool transformation callback was sent wrong value");
	ck_assert_msg(M_str_eq(mem_custom, "custom_value"), "custom transformation callback was sent wrong value");

	M_free(mem_strdup);
	M_free(mem_custom);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_transformation_default)
{
	const char *filename = "./tmp_conf_check_transformation_default.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that the correct default value is sent to the transformation callback. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), "buf_default_value", NULL, buf_default_value_cb);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, "strdup_default_value", NULL, strdup_default_value_cb);
	M_conf_register_int8(conf, "int8_key", &mem_int8, -123, 0, 0, int8_default_value_cb);
	M_conf_register_int16(conf, "int16_key", &mem_int16, -234, 0, 0, int16_default_value_cb);
	M_conf_register_int32(conf, "int32_key", &mem_int32, -345, 0, 0, int32_default_value_cb);
	M_conf_register_int64(conf, "int64_key", &mem_int64, -456, 0, 0, int64_default_value_cb);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 123, 0, 0, uint8_default_value_cb);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 234, 0, 0, uint16_default_value_cb);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 345, 0, 0, uint32_default_value_cb);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 456, 0, 0, uint64_default_value_cb);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 567, 0, 0, sizet_default_value_cb);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, bool_default_value_cb);

	ck_assert_msg(M_conf_parse(conf), "transformation callbacks failed transformation");

	ck_assert_msg(M_str_eq(mem_buf, "buf_default_value"), "buf transformation callback was sent wrong default value");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_default_value"), "strdup transformation callback was sent wrong default value");
	ck_assert_msg(mem_int8 == -123, "int8 transformation callback was sent wrong default value");
	ck_assert_msg(mem_int16 == -234, "int16 transformation callback was sent wrong default value");
	ck_assert_msg(mem_int32 == -345, "int32 transformation callback was sent wrong default value");
	ck_assert_msg(mem_int64 == -456, "int64 transformation callback was sent wrong default value");
	ck_assert_msg(mem_uint8 == 123, "uint8 transformation callback was sent wrong default value");
	ck_assert_msg(mem_uint16 == 234, "uint16 transformation callback was sent wrong default value");
	ck_assert_msg(mem_uint32 == 345, "uint32 transformation callback was sent wrong default value");
	ck_assert_msg(mem_uint64 == 456, "uint64 transformation callback was sent wrong default value");
	ck_assert_msg(mem_sizet == 567, "sizet transformation callback was sent wrong default value");
	ck_assert_msg(mem_bool == M_FALSE, "bool transformation callback was sent wrong default value");

	M_free(mem_strdup);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_no_block_on_error)
{
	const char *filename = "./tmp_conf_check_no_block_on_error.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;
	M_int64     mem_custom;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Check that one error during parsing doesn't block other registrations. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), NULL, "[:digit:]+", NULL); /* This one should fail. */
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, NULL);
	M_conf_register_custom(conf, "custom_key", &mem_custom, custom_real_cb);

	ck_assert_msg(!M_conf_parse(conf), "conf parse should have failed");

	ck_assert_msg(M_str_isempty(mem_buf), "buf registration should have failed");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup was blocked");
	ck_assert_msg(mem_int8 == -8, "int8 was blocked");
	ck_assert_msg(mem_int16 == -16, "int16 was blocked");
	ck_assert_msg(mem_int32 == -32, "int32 was blocked");
	ck_assert_msg(mem_int64 == -64, "int64 was blocked");
	ck_assert_msg(mem_uint8 == 8, "uint8 was blocked");
	ck_assert_msg(mem_uint16 == 16, "uint16 was blocked");
	ck_assert_msg(mem_uint32 == 32, "uint32 was blocked");
	ck_assert_msg(mem_uint64 == 64, "uint64 was blocked");
	ck_assert_msg(mem_sizet == 128, "sizet was blocked");
	ck_assert_msg(mem_bool == M_TRUE, "bool was blocked");
	ck_assert_msg(mem_custom == 999, "custom was blocked");

	M_free(mem_strdup);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_unused_single)
{
	/* Check that all unused keys in a configuration file with only single-value keys are accounted for. */
	const char    *filename = "./tmp_conf_check_unused_single.ini";
	M_conf_t      *conf     = NULL;

	char           mem_buf_1[64];
	char          *mem_strdup_1;
	M_int8         mem_int8_1;
	M_int16        mem_int16_1;
	M_int32        mem_int32_1;
	M_int64        mem_int64_1;
	M_uint8        mem_uint8_1;
	M_uint16       mem_uint16_1;
	M_uint32       mem_uint32_1;
	M_uint64       mem_uint64_1;
	size_t         mem_sizet_1;
	M_bool         mem_bool_1;
	M_int64        mem_custom_1;

	char           mem_buf_3[64];
	char          *mem_strdup_3;
	M_int8         mem_int8_3;
	M_int16        mem_int16_3;
	M_int32        mem_int32_3;
	M_int64        mem_int64_3;
	M_uint8        mem_uint8_3;
	M_uint16       mem_uint16_3;
	M_uint32       mem_uint32_3;
	M_uint64       mem_uint64_3;
	size_t         mem_sizet_3;
	M_bool         mem_bool_3;
	M_int64        mem_custom_3;

	M_list_str_t  *unused;
	M_hash_dict_t *expected_unused;
	size_t         i;

	ck_assert_msg(create_ini(filename, CONF_UNUSED_SINGLE), "failed to create temporary config file");

	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Register the first and third blocks of keys, and check that everything is ok. */
	M_conf_register_buf(conf, "buf_key1", mem_buf_1, sizeof(mem_buf_1), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key1", &mem_strdup_1, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key1", &mem_int8_1, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key1", &mem_int16_1, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key1", &mem_int32_1, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key1", &mem_int64_1, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key1", &mem_uint8_1, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key1", &mem_uint16_1, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key1", &mem_uint32_1, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key1", &mem_uint64_1, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key1", &mem_sizet_1, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key1", &mem_bool_1, M_FALSE, NULL);
	M_conf_register_custom(conf, "custom_key1", &mem_custom_1, custom_real_cb);

	M_conf_register_buf(conf, "buf_key3", mem_buf_3, sizeof(mem_buf_3), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key3", &mem_strdup_3, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key3", &mem_int8_3, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key3", &mem_int16_3, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key3", &mem_int32_3, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key3", &mem_int64_3, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key3", &mem_uint8_3, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key3", &mem_uint16_3, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key3", &mem_uint32_3, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key3", &mem_uint64_3, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key3", &mem_sizet_3, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key3", &mem_bool_3, M_FALSE, NULL);
	M_conf_register_custom(conf, "custom_key3", &mem_custom_3, custom_real_cb);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed");

	ck_assert_msg(M_str_eq(mem_buf_1, "buf_value"), "buf (1) registration failed");
	ck_assert_msg(M_str_eq(mem_strdup_1, "strdup_value"), "strdup (1) registration failed");
	ck_assert_msg(mem_int8_1 == -8, "int8 (1) registration failed");
	ck_assert_msg(mem_int16_1 == -16, "int16 (1) registration failed");
	ck_assert_msg(mem_int32_1 == -32, "int32 (1) registration failed");
	ck_assert_msg(mem_int64_1 == -64, "int64 (1) registration failed");
	ck_assert_msg(mem_uint8_1 == 8, "uint8 (1) registration failed");
	ck_assert_msg(mem_uint16_1 == 16, "uint16 (1) registration failed");
	ck_assert_msg(mem_uint32_1 == 32, "uint32 (1) registration failed");
	ck_assert_msg(mem_uint64_1 == 64, "uint64 (1) registration failed");
	ck_assert_msg(mem_sizet_1 == 128, "sizet (1) registration failed");
	ck_assert_msg(mem_bool_1 == M_TRUE, "bool (1) registration failed");
	ck_assert_msg(mem_custom_1 == 999, "custom (1) registration failed");

	ck_assert_msg(M_str_eq(mem_buf_3, "buf_value"), "buf (3) registration failed");
	ck_assert_msg(M_str_eq(mem_strdup_3, "strdup_value"), "strdup (3) registration failed");
	ck_assert_msg(mem_int8_3 == -8, "int8 (3) registration failed");
	ck_assert_msg(mem_int16_3 == -16, "int16 (3) registration failed");
	ck_assert_msg(mem_int32_3 == -32, "int32 (3) registration failed");
	ck_assert_msg(mem_int64_3 == -64, "int64 (3) registration failed");
	ck_assert_msg(mem_uint8_3 == 8, "uint8 (3) registration failed");
	ck_assert_msg(mem_uint16_3 == 16, "uint16 (3) registration failed");
	ck_assert_msg(mem_uint32_3 == 32, "uint32 (3) registration failed");
	ck_assert_msg(mem_uint64_3 == 64, "uint64 (3) registration failed");
	ck_assert_msg(mem_sizet_3 == 128, "sizet (3) registration failed");
	ck_assert_msg(mem_bool_3 == M_TRUE, "bool (3) registration failed");
	ck_assert_msg(mem_custom_3 == 999, "custom (3) registration failed");

	/* Now that we have used up those keys, let's make sure we have the correct unused keys left. */
	unused          = M_conf_unused_keys(conf);
	expected_unused = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	M_hash_dict_insert(expected_unused, "buf_key2", "buf_value");
	M_hash_dict_insert(expected_unused, "strdup_key2", "strdup_value");
	M_hash_dict_insert(expected_unused, "int8_key2", "-8");
	M_hash_dict_insert(expected_unused, "int16_key2", "-16");
	M_hash_dict_insert(expected_unused, "int32_key2", "-32");
	M_hash_dict_insert(expected_unused, "int64_key2", "-64");
	M_hash_dict_insert(expected_unused, "uint8_key2", "8");
	M_hash_dict_insert(expected_unused, "uint16_key2", "16");
	M_hash_dict_insert(expected_unused, "uint32_key2", "32");
	M_hash_dict_insert(expected_unused, "uint64_key2", "64");
	M_hash_dict_insert(expected_unused, "sizet_key2", "128");
	M_hash_dict_insert(expected_unused, "bool_key2", "yes");
	M_hash_dict_insert(expected_unused, "custom_key2", "custom_value");

	ck_assert_msg(M_list_str_len(unused) == M_hash_dict_num_keys(expected_unused),
			"mismatch in number of keys unused (expected %zu, have %zu)", M_hash_dict_num_keys(expected_unused), M_list_str_len(unused));

	for (i=0; i<M_list_str_len(unused); i++) {
		const char *key   = NULL;
		const char *value = NULL;

		key   = M_list_str_at(unused, i);
		value = M_hash_dict_get_direct(expected_unused, key);
		ck_assert_msg(!M_str_isempty(value), "unexpected unused key: %s", key);
		ck_assert_msg(M_str_eq(value, M_conf_get_value(conf, key)), "unused key %s has wrong value (%s)", key, value);
		M_hash_dict_remove(expected_unused, key);
	}

	ck_assert_msg(M_hash_dict_num_keys(expected_unused) == 0, "not all unused keys were accounted for");

	M_hash_dict_destroy(expected_unused);
	M_list_str_destroy(unused);
	M_free(mem_strdup_3);
	M_free(mem_strdup_1);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_unused_multi)
{
	/* Check that all unused keys in a configuration file with multi-value keys are accounted for. */
	const char   *filename = "./tmp_conf_check_unused_multi.ini";
	M_conf_t     *conf     = NULL;
	char          mem_buf[64];
	char         *mem_strdup;
	M_int8        mem_int8;
	M_int16       mem_int16;
	M_int32       mem_int32;
	M_int64       mem_int64;
	M_uint8       mem_uint8;
	M_uint16      mem_uint16;
	M_uint32      mem_uint32;
	M_uint64      mem_uint64;
	size_t        mem_sizet;
	M_bool        mem_bool;
	M_int64       mem_custom;
	M_list_str_t *unused;
	size_t        count;

	ck_assert_msg(create_ini(filename, CONF_UNUSED_MULTI), "failed to create temporary config file");

	conf = M_conf_create(filename, M_TRUE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	/* Register every key once. Then we'll make sure that what remains is correct. */
	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, NULL);
	M_conf_register_custom(conf, "custom_key", &mem_custom, custom_real_cb);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed");

	ck_assert_msg(M_str_eq(mem_buf, "buf_value"), "buf registration failed");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup registration failed");
	ck_assert_msg(mem_int8 == -8, "int8 registration failed");
	ck_assert_msg(mem_int16 == -16, "int16 registration failed");
	ck_assert_msg(mem_int32 == -32, "int32 registration failed");
	ck_assert_msg(mem_int64 == -64, "int64 registration failed");
	ck_assert_msg(mem_uint8 == 8, "uint8 registration failed");
	ck_assert_msg(mem_uint16 == 16, "uint16 registration failed");
	ck_assert_msg(mem_uint32 == 32, "uint32 registration failed");
	ck_assert_msg(mem_uint64 == 64, "uint64 registration failed");
	ck_assert_msg(mem_sizet == 128, "sizet registration failed");
	ck_assert_msg(mem_bool == M_TRUE, "bool registration failed");
	ck_assert_msg(mem_custom == 999, "custom registration failed");

	/* Now that we have used up those keys, let's make sure we have the correct unused keys left. We're going to do
 	 * everything manually to make it easier to see. */
	unused = M_conf_unused_keys(conf);
	ck_assert_msg(M_list_str_len(unused) == 15, "mismatch in number of keys unused (have %zu, want 15)", M_list_str_len(unused));

	count = M_list_str_count(unused, "strdup_key", M_LIST_STR_MATCH_VAL);
	ck_assert_msg(count == 1, "strdup count wrong (want 1, have %zu", count);

	count = M_list_str_count(unused, "int8_key", M_LIST_STR_MATCH_VAL);
	ck_assert_msg(count == 2, "int8 count wrong (want 2, have %zu", count);

	count = M_list_str_count(unused, "int16_key", M_LIST_STR_MATCH_VAL);
	ck_assert_msg(count == 3, "int16 count wrong (want 3, have %zu", count);

	count = M_list_str_count(unused, "int32_key", M_LIST_STR_MATCH_VAL);
	ck_assert_msg(count == 4, "int32 count wrong (want 4, have %zu", count);

	count = M_list_str_count(unused, "int64_key", M_LIST_STR_MATCH_VAL);
	ck_assert_msg(count == 5, "int64 count wrong (want 5, have %zu", count);

	M_list_str_destroy(unused);
	M_free(mem_strdup);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

START_TEST(check_validators)
{
	/* Check that the post-parse validators are good. */
	const char *filename = "./tmp_conf_check_validators.ini";
	M_conf_t   *conf     = NULL;
	char        mem_buf[64];
	char       *mem_strdup;
	M_int8      mem_int8;
	M_int16     mem_int16;
	M_int32     mem_int32;
	M_int64     mem_int64;
	M_uint8     mem_uint8;
	M_uint16    mem_uint16;
	M_uint32    mem_uint32;
	M_uint64    mem_uint64;
	size_t      mem_sizet;
	M_bool      mem_bool;

	ck_assert_msg(create_ini(filename, CONF_REGISTRATIONS), "failed to create temporary config file");

	/* These should pass. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 0, 0, SIZE_MAX, NULL);

	M_conf_register_validator(conf, validate_buf_cb, mem_buf);
	M_conf_register_validator(conf, validate_strdup_cb, &mem_strdup);
	M_conf_register_validator(conf, validate_int8_cb, &mem_int8);
	M_conf_register_validator(conf, validate_uint8_cb, &mem_uint8);
	M_conf_register_validator(conf, validate_sizet_cb, &mem_sizet);

	ck_assert_msg(M_conf_parse(conf), "conf parse failed");

	ck_assert_msg(M_str_eq(mem_buf, "buf_value"), "buf failed to get conf value");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup failed to get conf value");
	ck_assert_msg(mem_int8 == -8, "int8 failed to get conf value");
	ck_assert_msg(mem_uint8 == 8, "uint8 failed to get conf value");
	ck_assert_msg(mem_sizet == 128, "sizet failed to get conf value");

	M_free(mem_strdup);
	M_conf_destroy(conf); conf = NULL;



	/* These should fail. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, NULL);

	M_conf_register_validator(conf, validate_int16_cb, &mem_int16);
	M_conf_register_validator(conf, validate_uint16_cb, &mem_uint16);
	M_conf_register_validator(conf, validate_bool_cb, &mem_bool);

	ck_assert_msg(!M_conf_parse(conf), "conf parse succeeded");

	ck_assert_msg(mem_int16 == -16, "int16 failed to get conf value");
	ck_assert_msg(mem_uint16 == 16, "uint16 failed to get conf value");
	ck_assert_msg(mem_bool == M_TRUE, "bool failed to get conf value");

	M_conf_destroy(conf); conf = NULL;



	/* Even though validation fails, all values should still be set correctly. */
	conf = M_conf_create(filename, M_FALSE, NULL, 0);
	ck_assert_msg(conf != NULL, "could not read %s", filename);

	M_conf_register_buf(conf, "buf_key", mem_buf, sizeof(mem_buf), NULL, NULL, NULL);
	M_conf_register_strdup(conf, "strdup_key", &mem_strdup, NULL, NULL, NULL);
	M_conf_register_int8(conf, "int8_key", &mem_int8, 0, M_INT8_MIN, M_INT8_MAX, NULL);
	M_conf_register_uint8(conf, "uint8_key", &mem_uint8, 0, 0, M_UINT8_MAX, NULL);
	M_conf_register_int16(conf, "int16_key", &mem_int16, 0, M_INT16_MIN, M_INT16_MAX, NULL);
	M_conf_register_uint16(conf, "uint16_key", &mem_uint16, 0, 0, M_UINT16_MAX, NULL);
	M_conf_register_int32(conf, "int32_key", &mem_int32, 0, M_INT32_MIN, M_INT32_MAX, NULL);
	M_conf_register_int64(conf, "int64_key", &mem_int64, 0, M_INT64_MIN, M_INT64_MAX, NULL);
	M_conf_register_uint32(conf, "uint32_key", &mem_uint32, 0, 0, M_UINT32_MAX, NULL);
	M_conf_register_uint64(conf, "uint64_key", &mem_uint64, 0, 0, M_UINT64_MAX, NULL);
	M_conf_register_sizet(conf, "sizet_key", &mem_sizet, 0, 0, SIZE_MAX, NULL);
	M_conf_register_bool(conf, "bool_key", &mem_bool, M_FALSE, NULL);

	M_conf_register_validator(conf, validate_buf_cb, mem_buf);
	M_conf_register_validator(conf, validate_strdup_cb, &mem_strdup);
	M_conf_register_validator(conf, validate_int8_cb, &mem_int8);
	M_conf_register_validator(conf, validate_uint8_cb, &mem_uint8);
	M_conf_register_validator(conf, validate_int16_cb, &mem_int16);
	M_conf_register_validator(conf, validate_uint16_cb, &mem_uint16);
	M_conf_register_validator(conf, validate_bool_cb, &mem_bool);

	ck_assert_msg(!M_conf_parse(conf), "conf parse succeeded");

	ck_assert_msg(M_str_eq(mem_buf, "buf_value"), "buf failed to get conf value");
	ck_assert_msg(M_str_eq(mem_strdup, "strdup_value"), "strdup failed to get conf value");
	ck_assert_msg(mem_int8 == -8, "int8 failed to get conf value");
	ck_assert_msg(mem_uint8 == 8, "uint8 failed to get conf value");
	ck_assert_msg(mem_int16 == -16, "int16 failed to get conf value");
	ck_assert_msg(mem_uint16 == 16, "uint16 failed to get conf value");
	ck_assert_msg(mem_int32 == -32, "int32 failed to get conf value");
	ck_assert_msg(mem_int64 == -64, "int64 failed to get conf value");
	ck_assert_msg(mem_uint32 == 32, "uint32 failed to get conf value");
	ck_assert_msg(mem_uint64 == 64, "uint64 failed to get conf value");
	ck_assert_msg(mem_sizet == 128, "sizet failed to get conf value");
	ck_assert_msg(mem_bool == M_TRUE, "bool failed to get conf value");

	M_free(mem_strdup);
	M_conf_destroy(conf);

	ck_assert_msg(remove_ini(filename), "failed to remove temporary config file");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_conf_suite(void)
{
	Suite *suite;
	TCase *check_missing_path_test;
	TCase *check_missing_file_test;
	TCase *check_missing_errbuf_test;
	TCase *check_create_single_value_test;
	TCase *check_create_multiple_value_test;
	TCase *check_fail_multiple_value_test;
	TCase *check_sections_test;
	TCase *check_no_sections_test;
	TCase *check_sections_no_multi_test;
	TCase *check_single_value_test;
	TCase *check_multiple_values_test;
	TCase *check_invalid_registration_test;
	TCase *check_registration_args_test;
	TCase *check_straight_registration_test;
	TCase *check_sanity_test;
	TCase *check_negatives_test;
	TCase *check_under_min_possible_test;
	TCase *check_over_max_possible_test;
	TCase *check_transformation_error_test;
	TCase *check_transformation_override_test;
	TCase *check_transformation_set_test;
	TCase *check_transformation_value_test;
	TCase *check_transformation_default_test;
	TCase *check_no_block_on_error_test;
	TCase *check_unused_single_test;
	TCase *check_unused_multi_test;
	TCase *check_validators_test;

	suite = suite_create("conf");

	check_missing_path_test = tcase_create("check_missing_path");
	tcase_add_unchecked_fixture(check_missing_path_test, NULL, NULL);
	tcase_add_test(check_missing_path_test, check_missing_path);
	suite_add_tcase(suite, check_missing_path_test);

	check_missing_file_test = tcase_create("check_missing_file");
	tcase_add_unchecked_fixture(check_missing_file_test, NULL, NULL);
	tcase_add_test(check_missing_file_test, check_missing_file);
	suite_add_tcase(suite, check_missing_file_test);

	check_missing_errbuf_test = tcase_create("check_missing_errbuf");
	tcase_add_unchecked_fixture(check_missing_errbuf_test, NULL, NULL);
	tcase_add_test(check_missing_errbuf_test, check_missing_errbuf);
	suite_add_tcase(suite, check_missing_errbuf_test);

	check_create_single_value_test = tcase_create("check_create_single_value");
	tcase_add_unchecked_fixture(check_create_single_value_test, NULL, NULL);
	tcase_add_test(check_create_single_value_test, check_create_single_value);
	suite_add_tcase(suite, check_create_single_value_test);

	check_create_multiple_value_test = tcase_create("check_create_multiple_values");
	tcase_add_unchecked_fixture(check_create_multiple_value_test, NULL, NULL);
	tcase_add_test(check_create_multiple_value_test, check_create_multiple_values);
	suite_add_tcase(suite, check_create_multiple_value_test);

	check_fail_multiple_value_test = tcase_create("check_fail_multiple_values");
	tcase_add_unchecked_fixture(check_fail_multiple_value_test, NULL, NULL);
	tcase_add_test(check_fail_multiple_value_test, check_fail_multiple_values);
	suite_add_tcase(suite, check_fail_multiple_value_test);

	check_sections_test = tcase_create("check_sections");
	tcase_add_unchecked_fixture(check_sections_test, NULL, NULL);
	tcase_add_test(check_sections_test, check_sections);
	suite_add_tcase(suite, check_sections_test);

	check_no_sections_test = tcase_create("check_no_sections");
	tcase_add_unchecked_fixture(check_no_sections_test, NULL, NULL);
	tcase_add_test(check_no_sections_test, check_no_sections);
	suite_add_tcase(suite, check_no_sections_test);

	check_sections_no_multi_test = tcase_create("check_sections_no_multi");
	tcase_add_unchecked_fixture(check_sections_no_multi_test, NULL, NULL);
	tcase_add_test(check_sections_no_multi_test, check_sections_no_multi);
	suite_add_tcase(suite, check_sections_no_multi_test);

	check_single_value_test = tcase_create("check_single_value");
	tcase_add_unchecked_fixture(check_single_value_test, NULL, NULL);
	tcase_add_test(check_single_value_test, check_single_value);
	suite_add_tcase(suite, check_single_value_test);

	check_multiple_values_test = tcase_create("check_multiple_values");
	tcase_add_unchecked_fixture(check_multiple_values_test, NULL, NULL);
	tcase_add_test(check_multiple_values_test, check_multiple_values);
	suite_add_tcase(suite, check_multiple_values_test);

	check_invalid_registration_test = tcase_create("check_invalid_registration");
	tcase_add_unchecked_fixture(check_invalid_registration_test, NULL, NULL);
	tcase_add_test(check_invalid_registration_test, check_invalid_registration);
	suite_add_tcase(suite, check_invalid_registration_test);

	check_registration_args_test = tcase_create("check_registration_args");
	tcase_add_unchecked_fixture(check_registration_args_test, NULL, NULL);
	tcase_add_test(check_registration_args_test, check_registration_args);
	suite_add_tcase(suite, check_registration_args_test);

	check_straight_registration_test = tcase_create("check_straight_registration");
	tcase_add_unchecked_fixture(check_straight_registration_test, NULL, NULL);
	tcase_add_test(check_straight_registration_test, check_straight_registration);
	suite_add_tcase(suite, check_straight_registration_test);

	check_sanity_test = tcase_create("check_sanity");
	tcase_add_unchecked_fixture(check_sanity_test, NULL, NULL);
	tcase_add_test(check_sanity_test, check_sanity);
	suite_add_tcase(suite, check_sanity_test);

	check_negatives_test = tcase_create("check_negatives");
	tcase_add_unchecked_fixture(check_negatives_test, NULL, NULL);
	tcase_add_test(check_negatives_test, check_negatives);
	suite_add_tcase(suite, check_negatives_test);

	check_under_min_possible_test = tcase_create("check_under_min_possible");
	tcase_add_unchecked_fixture(check_under_min_possible_test, NULL, NULL);
	tcase_add_test(check_under_min_possible_test, check_under_min_possible);
	suite_add_tcase(suite, check_under_min_possible_test);

	check_over_max_possible_test = tcase_create("check_over_max_possible");
	tcase_add_unchecked_fixture(check_over_max_possible_test, NULL, NULL);
	tcase_add_test(check_over_max_possible_test, check_over_max_possible);
	suite_add_tcase(suite, check_over_max_possible_test);

	check_transformation_error_test = tcase_create("check_transformation_error");
	tcase_add_unchecked_fixture(check_transformation_error_test, NULL, NULL);
	tcase_add_test(check_transformation_error_test, check_transformation_error);
	suite_add_tcase(suite, check_transformation_error_test);

	check_transformation_override_test = tcase_create("check_transformation_override");
	tcase_add_unchecked_fixture(check_transformation_override_test, NULL, NULL);
	tcase_add_test(check_transformation_override_test, check_transformation_override);
	suite_add_tcase(suite, check_transformation_override_test);

	check_transformation_set_test = tcase_create("check_transformation_set");
	tcase_add_unchecked_fixture(check_transformation_set_test, NULL, NULL);
	tcase_add_test(check_transformation_set_test, check_transformation_set);
	suite_add_tcase(suite, check_transformation_set_test);

	check_transformation_value_test = tcase_create("check_transformation_value");
	tcase_add_unchecked_fixture(check_transformation_value_test, NULL, NULL);
	tcase_add_test(check_transformation_value_test, check_transformation_value);
	suite_add_tcase(suite, check_transformation_value_test);

	check_transformation_default_test = tcase_create("check_transformation_default");
	tcase_add_unchecked_fixture(check_transformation_default_test, NULL, NULL);
	tcase_add_test(check_transformation_default_test, check_transformation_default);
	suite_add_tcase(suite, check_transformation_default_test);

	check_no_block_on_error_test = tcase_create("check_no_block_on_error");
	tcase_add_unchecked_fixture(check_no_block_on_error_test, NULL, NULL);
	tcase_add_test(check_no_block_on_error_test, check_no_block_on_error);
	suite_add_tcase(suite, check_no_block_on_error_test);

	check_unused_single_test = tcase_create("check_unused_single");
	tcase_add_unchecked_fixture(check_unused_single_test, NULL, NULL);
	tcase_add_test(check_unused_single_test, check_unused_single);
	suite_add_tcase(suite, check_unused_single_test);

	check_unused_multi_test = tcase_create("check_unused_multi");
	tcase_add_unchecked_fixture(check_unused_multi_test, NULL, NULL);
	tcase_add_test(check_unused_multi_test, check_unused_multi);
	suite_add_tcase(suite, check_unused_multi_test);

	check_validators_test = tcase_create("check_validators");
	tcase_add_unchecked_fixture(check_validators_test, NULL, NULL);
	tcase_add_test(check_validators_test, check_validators);
	suite_add_tcase(suite, check_validators_test);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_conf_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_conf.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
