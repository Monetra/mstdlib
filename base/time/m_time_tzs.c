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

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_time_tzs {
	M_hash_strvp_t         *tzs;
	M_hash_dict_t          *alias;
	/* Lazy loading. */
	void                   *lazy_data;
	void                    (*lazy_data_destroy)(void *data);
	M_time_tzs_lazy_load_t  lazy_load;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_time_tz_destroy_vp(void *data)
{
	M_time_tz_destroy(data);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * M_time_tz_t */

void M_time_tz_destroy(M_time_tz_t *tz)
{
	if (tz == NULL)
		return;
	tz->destroy(tz->data);
	M_free(tz);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_tzs_t *M_time_tzs_create(void)
{
	M_time_tzs_t *tzs;

	tzs = M_malloc_zero(sizeof(*tzs));

	tzs->tzs   = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP, M_time_tz_destroy_vp);
	tzs->alias = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);

	return tzs;
}

void M_time_tzs_destroy(M_time_tzs_t *tzs)
{
	if (tzs == NULL)
		return;

	M_hash_strvp_destroy(tzs->tzs, M_TRUE);
	tzs->tzs = NULL;

	M_hash_dict_destroy(tzs->alias);
	tzs->alias = NULL;

	if (tzs->lazy_data_destroy != NULL) {
		tzs->lazy_data_destroy(tzs->lazy_data);
	}
	tzs->lazy_data         = NULL;
	tzs->lazy_data_destroy = NULL;

	M_free(tzs);
}

const M_time_tz_t *M_time_tzs_get_tz(M_time_tzs_t *tzs, const char *name)
{
	const char  *real_name;
	M_time_tz_t *tz;

	if (tzs == NULL)
		return NULL;

	/* Get the real name. */
	if (!M_hash_dict_get(tzs->alias, name, &real_name)) {
		return NULL;
	}

	tz = (M_time_tz_t *)M_hash_strvp_get_direct(tzs->tzs, real_name);
	/* If there is no tz (but there was an alis we're probably using lazy loading. */
	if (tz == NULL && tzs->lazy_load != NULL) {
		/* Load the tz. */
		tz = tzs->lazy_load(real_name, tzs->lazy_data);
		if (!M_time_tzs_add_tz(tzs, tz, real_name)) {
			M_time_tz_destroy(tz);
			tz = NULL;
		}
	}

	return tz;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_load_source_t M_time_tzs_load(M_time_tzs_t **tzs, M_uint32 zones, M_uint32 alias_f, M_uint32 flags)
{
	if (tzs == NULL)
		return M_TIME_LOAD_SOURCE_FAIL;

#ifndef _WIN32
	*tzs = M_time_tzs_load_win_zones(zones, alias_f, flags);
#else
	*tzs = M_time_tzs_load_zoneinfo(NULL, zones, alias_f, flags);
#endif

	/* Loaded system zones. */
	if (*tzs != NULL && M_hash_strvp_num_keys((*tzs)->tzs) > 0)
		return M_TIME_LOAD_SOURCE_SYSTEM;

	M_time_tzs_destroy(*tzs);

	/* Generic US not in requested zones. */
	if (zones != M_TIME_TZ_ZONE_ALL && !(zones & M_TIME_TZ_ZONE_AMERICA))
		return M_TIME_LOAD_SOURCE_FAIL;

	*tzs = M_time_tzs_create();
	/* Main US timezones. DST rules implemented in 2007. */
	M_time_tzs_add_posix_str(*tzs, "EST4EDT,M3.2.0/02:00:00,M11.1.0/02:00:00");
	M_time_tzs_add_posix_str(*tzs, "CST5CDT,M3.2.0/02:00:00,M11.1.0/02:00:00");
	M_time_tzs_add_posix_str(*tzs, "MST6MDT,M3.2.0/02:00:00,M11.1.0/02:00:00");
	M_time_tzs_add_posix_str(*tzs, "PST7PDT,M3.2.0/02:00:00,M11.1.0/02:00:00");

	return M_TIME_LOAD_SOURCE_FALLBACK;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef _WIN32
/* For Windows these are implemented in m_time_tz_win.c.
 * Other OS's can't use these functions so they will
 * return an invalid response. */
M_time_tzs_t *M_time_tzs_load_win_zones(M_uint32 zones, M_uint32 alias_f, M_uint32 flags)
{
	(void)zones;
	(void)alias_f;
	(void)flags;
	return NULL;
}

M_time_result_t M_time_tzs_add_win_zone(M_time_tzs_t *tzs, const char *name)
{
	(void)tzs;
	(void)name;
	return M_TIME_RESULT_INVALID;
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_time_tzs_add_tz(M_time_tzs_t *tzs, M_time_tz_t *tz, const char *name)
{
	void *stored_tz = NULL;

	if (tzs == NULL || name == NULL || *name == '\0')
		return M_FALSE;

	/* Check if there is already a tz (not just a NULL placeholder) for the given name. */
	if (M_hash_strvp_get(tzs->tzs, name, &stored_tz) && stored_tz != NULL)
		return M_FALSE;

	/* Add the tz. */
	M_hash_strvp_insert(tzs->tzs, name, tz);

	return M_TRUE;
}

M_bool M_time_tzs_add_alias(M_time_tzs_t *tzs, const char *alias, const char *name)
{
	if (tzs == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(tzs->tzs, name, NULL))
		return M_FALSE;

	M_hash_dict_insert(tzs->alias, alias, name);
	return M_TRUE;
}

void M_time_tzs_set_lazy_load(M_time_tzs_t *tzs, M_time_tzs_lazy_load_t func, void *data, void (*data_destroy)(void *data))
{
	if (tzs == NULL)
		return;

	tzs->lazy_load         = func;
	tzs->lazy_data         = data;
	tzs->lazy_data_destroy = data_destroy;
}

static M_list_str_t *M_time_tzs_get_loaded_int(M_hash_dict_t *d)
{
	M_hash_dict_enum_t *dictenum;
	M_list_str_t       *names   = NULL;
	const char         *key;

	if (d == NULL)
		return NULL;

	if (M_hash_dict_enumerate(d, &dictenum) > 0) {
		names = M_list_str_create(M_LIST_STR_SORTASC);
	}
	while (M_hash_dict_enumerate_next(d, dictenum, &key, NULL)) {
		M_list_str_insert(names, key);
	}
	M_hash_dict_enumerate_free(dictenum);

	return names;
}

M_list_str_t *M_time_tzs_get_loaded_zones(const M_time_tzs_t *tzs)
{
	if (tzs == NULL)
		return NULL;
	return M_time_tzs_get_loaded_int(tzs->alias);
}

M_bool M_time_tzs_merge(M_time_tzs_t **dest, M_time_tzs_t *src, char **err_name)
{
	M_hash_strvp_enum_t *tz_enum;
	const char          *key;

	if (src == NULL) {
		return M_FALSE;
	}
	if (*dest == NULL) {
		*dest = src;
		return M_TRUE;
	}

	/* Check that there are no duplicates and we can merge. */
	M_hash_strvp_enumerate(src->tzs, &tz_enum);
	while (M_hash_strvp_enumerate_next(src->tzs, tz_enum, &key, NULL)) {
		if (M_hash_strvp_get((*dest)->tzs, key, NULL)) {
			if (err_name != NULL) {
				*err_name = M_strdup(key);
			}
			M_hash_strvp_enumerate_free(tz_enum);
			return M_FALSE;
		}
	}
	M_hash_strvp_enumerate_free(tz_enum);

	/* Merge in tz. */
	M_hash_strvp_merge(&((*dest)->tzs), src->tzs);
	src->tzs = NULL;

	/* Merge in aliases. */
	M_hash_dict_merge(&((*dest)->alias), src->alias);
	src->alias = NULL;

	M_time_tzs_destroy(src);

	return M_TRUE;
}

