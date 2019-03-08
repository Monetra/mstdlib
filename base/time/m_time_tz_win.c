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
#include "platform/m_platform.h"
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define MAX_KEY_LENGTH 255
#define M_TIME_TZ_WIN_ZONE_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct _REG_TZI_FORMAT {
    LONG Bias;
    LONG StandardBias;
    LONG DaylightBias;
    SYSTEMTIME StandardDate;
    SYSTEMTIME DaylightDate;
} REG_TZI_FORMAT;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_str_t *M_time_tz_win_list_zones(void)
{
	M_list_str_t *zones;
	HKEY          zone_key;
	DWORD         i = 0;
	char          name[MAX_KEY_LENGTH+1];
	DWORD         name_len;
	LONG          ret;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(M_TIME_TZ_WIN_ZONE_KEY), 0, KEY_READ, &zone_key) != ERROR_SUCCESS) {
		return NULL;
	}

	zones    = M_list_str_create(M_LIST_STR_SORTASC);
	name_len = sizeof(name);
	while ((ret = RegEnumKeyEx(zone_key, i, name, &name_len, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS) {
		M_list_str_insert(zones, name);
		name_len = sizeof(name);
		i++;
	}
	RegCloseKey(zone_key);
	if (ret != ERROR_NO_MORE_ITEMS) {
		M_list_str_destroy(zones);
		zones = NULL;
	}

	return zones;
}

static M_time_tz_dst_rule_t *M_time_tz_win_read_adjust(HKEY *key, const char *name, M_int64 year)
{
	M_time_tz_dst_rule_t *adjust;
	REG_TZI_FORMAT        tzi_data;
	DWORD                 tzi_data_len;

	if (key == NULL || name == NULL)
		return NULL;

	tzi_data_len = sizeof(tzi_data);
	if (RegQueryValueEx(*key, TEXT(name), NULL, NULL, (LPBYTE)&tzi_data, &tzi_data_len) != ERROR_SUCCESS) {
		return NULL;
	}

	adjust              = M_malloc_zero(sizeof(*adjust));
	adjust->year        = year;
	adjust->offset      = (tzi_data.Bias*-60)+(tzi_data.StandardBias*-60);
	adjust->offset_dst  = (tzi_data.Bias*-60)+(tzi_data.DaylightBias*-60);

	adjust->start.month = tzi_data.DaylightDate.wMonth;
	adjust->start.wday  = tzi_data.DaylightDate.wDayOfWeek;
	adjust->start.occur = tzi_data.DaylightDate.wDay;
	adjust->start.hour  = tzi_data.DaylightDate.wHour;
	adjust->start.min   = tzi_data.DaylightDate.wMinute;
	adjust->start.sec   = tzi_data.DaylightDate.wSecond;

	adjust->end.month   = tzi_data.StandardDate.wMonth;
	adjust->end.wday    = tzi_data.StandardDate.wDayOfWeek;
	adjust->end.occur   = tzi_data.StandardDate.wDay;
	adjust->end.hour    = tzi_data.StandardDate.wHour;
	adjust->end.min     = tzi_data.StandardDate.wMinute;
	adjust->end.sec     = tzi_data.StandardDate.wSecond;

	return adjust;
}

static M_time_tz_rule_t *M_time_tz_win_load_data(const char *name)
{
	M_time_tz_rule_t *rtz         = NULL;
	char              key_name[MAX_KEY_LENGTH+1];
	char              key_name_ddst[MAX_KEY_LENGTH+1];
	HKEY              key;
	HKEY              key_ddst;
	char              temp[33];
	DWORD             temp_len;
	DWORD             first_entry = 0;
	DWORD             last_entry  = 0;

	M_snprintf(key_name, sizeof(key_name), "%s\\%s", M_TIME_TZ_WIN_ZONE_KEY, name);
	M_snprintf(key_name_ddst, sizeof(key_name_ddst), "%s\\%s", key_name, "Dynamic DST");

	/* Open the main key so we verify it exists. */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(key_name), 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return NULL;
	}

	rtz       = M_time_tz_rule_create();
	rtz->name = M_strdup(name);

	/* Abbr STD */
	temp_len = sizeof(temp)-1;
	if (RegQueryValueEx(key, TEXT("Std"), NULL, NULL, (LPBYTE)&temp, &temp_len) != ERROR_SUCCESS) {
		RegCloseKey(key);
		return NULL;
	}
	temp[temp_len] = '\0';
	rtz->abbr = M_strdup(temp);

	/* Abbr DST */
	temp_len = sizeof(temp)-1;
	if (RegQueryValueEx(key, TEXT("Dlt"), NULL, NULL, (LPBYTE)&temp, &temp_len) != ERROR_SUCCESS) {
		RegCloseKey(key);
		return NULL;
	}
	temp[temp_len] = '\0';
	rtz->abbr_dst = M_strdup(temp);

	/* Load Dynamic DST data if it exists */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(key_name_ddst), 0, KEY_READ, &key_ddst) == ERROR_SUCCESS) {
		temp_len = sizeof(first_entry);
		if (RegQueryValueEx(key, TEXT("FirstEntry"), NULL, NULL, (LPBYTE)&first_entry, &temp_len) != ERROR_SUCCESS) {
			first_entry = 0;
		}
		temp_len = sizeof(last_entry);
		if (RegQueryValueEx(key, TEXT("LastEntry"), NULL, NULL, (LPBYTE)&last_entry, &temp_len) != ERROR_SUCCESS) {
			last_entry = 0;
		}
		for ( ; first_entry <= last_entry; first_entry++) {
			M_snprintf(temp, sizeof(temp), "%d", first_entry);
			M_time_tz_rule_add_dst_adjust(rtz, M_time_tz_win_read_adjust(&key_ddst, temp, first_entry));
		}
		RegCloseKey(key_ddst);
	} 

	/* Read the default TZI data if there was no dynamic dst data. */
	if (M_time_tz_dst_rules_len(rtz->adjusts) == 0) {
		M_time_tz_rule_add_dst_adjust(rtz, M_time_tz_win_read_adjust(&key, "TZI", 0));
	}

	RegCloseKey(key);
	return rtz;
}

static M_time_tz_t *M_time_tz_win_load(const char *name)
{
	M_time_tz_t      *tz;
	M_time_tz_rule_t *rtz;

	if (name == NULL || *name == '\0')
		return NULL;

	rtz = M_time_tz_win_load_data(name);
	if (rtz == NULL)
		return NULL;

	tz = M_time_tz_rule_create_tz(rtz);
	return tz;
}

/*! Lazy load Windows timezone data. */
static M_time_tz_t *M_time_tz_win_lazy_load_data(const char *name, void *data)
{
	(void)data;
	return M_time_tz_win_load(name);
}

static void M_time_tz_win_win_aliases_destroy_vp(void *data)
{
	M_list_str_destroy(data);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_result_t M_time_tzs_add_win_zone(M_time_tzs_t *tzs, const char *name)
{
	M_time_tz_t *tz;

	if (tzs == NULL || name == NULL || *name == '\0')
		return M_TIME_RESULT_INVALID;

	tz = M_time_tz_win_load(name);
	if (tz == NULL) {
		return M_TIME_RESULT_ERROR;
	}

	if (!M_time_tzs_add_tz(tzs, tz, name)) {
		M_time_tz_destroy(tz);
		return M_TIME_RESULT_DUP;
	}
	M_time_tzs_add_alias(tzs, name, name);

	return M_TIME_RESULT_SUCCESS;
}

M_time_tzs_t *M_time_tzs_load_win_zones(M_uint32 zones, M_uint32 alias_f, M_uint32 flags)
{
	M_time_tzs_t         *tzs;
	M_time_tz_t          *tz;
	M_list_str_t         *zones_win;
	M_list_str_t         *alias_list;
	M_hash_strvp_t       *win_map;
	M_hash_strvp_t       *win_alias;
	const char           *zone;
	M_time_tz_info_map_t *map_entry;
	size_t                len;
	size_t                alen;
	size_t                i;
	size_t                j;

	tzs = M_time_tzs_create();
	if (flags & M_TIME_TZ_LOAD_LAZY) {
		M_time_tzs_set_lazy_load(tzs, M_time_tz_win_lazy_load_data, NULL, NULL);
	}

	zones_win = M_time_tz_win_list_zones();
	win_map   = M_hash_strvp_create(500, 75, M_HASH_STRVP_CASECMP, NULL);
	win_alias = M_hash_strvp_create(500, 75, M_HASH_STRVP_CASECMP, M_time_tz_win_win_aliases_destroy_vp);

	/* Create an easier to use mapping of our alias table so we can deal with it in an easier manner. */
	for (i=0; M_time_tz_zone_map[i].olson_name != NULL; i++) {
		map_entry = &M_time_tz_zone_map[i];

		/* Filter out zone's we're not supporting. */
		if (zones != M_TIME_TZ_ZONE_ALL && !(zones & map_entry->zone)) {
			continue;
		}

		/* Add the entry. */
		M_hash_strvp_insert(win_map, map_entry->win_name, map_entry);

		/* Check that we have an alias list and if not create it. */
		if (!M_hash_strvp_get(win_alias, map_entry->win_name, (void **)&alias_list)) {
			alias_list = M_list_str_create(M_LIST_STR_SORTASC);
			M_hash_strvp_insert(win_alias, map_entry->win_name, alias_list);
		}

		/* Add the appropriate aliases. */
		if (alias_f == M_TIME_TZ_ALIAS_ALL || alias_f & M_TIME_TZ_ALIAS_OLSON_ALL || (alias_f & M_TIME_TZ_ALIAS_OLSON_MAIN && map_entry->main)) {
			M_list_str_insert(alias_list, map_entry->olson_name);
		}
		if (alias_f == M_TIME_TZ_ALIAS_ALL || alias_f & M_TIME_TZ_ALIAS_WINDOWS_ALL || (alias_f & M_TIME_TZ_ALIAS_WINDOWS_MAIN && map_entry->main)) {
			M_list_str_insert(alias_list, map_entry->win_name);
		}
	}

	/* Load the loaded zones. */
	len = M_list_str_len(zones_win);
	for (i=0; i<len; i++) {
		zone = M_list_str_at(zones_win, i);

		/* This is a special case. We don't want to exclude zones that are not in our mapping if we're loading
 		 * everything and using Windows names. */
		if (zones != M_TIME_TZ_ZONE_ALL && alias_f != M_TIME_TZ_ALIAS_ALL && !(alias_f & M_TIME_TZ_ALIAS_WINDOWS_ALL) && !M_hash_strvp_get(win_map, zone, (void **)&map_entry)) {
			continue;
		}

		/* Add zone. */
		if (flags & M_TIME_TZ_LOAD_LAZY) {
			M_time_tzs_add_tz(tzs, NULL, zone);
		} else {
			/* Check if the tz was already loaded. If we're not doing lazy loading the lazy function won't be
 			 * set so we can safely load this ourselves here. */
			if (M_time_tzs_get_tz(tzs, zone) == NULL) {
				/* Load the tz. */
				tz = M_time_tz_win_load(zone);
				if (tz != NULL && !M_time_tzs_add_tz(tzs, tz, zone)) {
					M_time_tz_destroy(tz);
					continue;
				}
			}
		}

		/* Add alias. */
		alias_list = M_hash_strvp_get_direct(win_alias, zone);
		alen       = M_list_str_len(alias_list);
		if (alen == 0) {
			M_time_tzs_add_alias(tzs, zone, zone);
		} else {
			for (j=0; j<alen; j++) {
				M_time_tzs_add_alias(tzs, M_list_str_at(alias_list, j), zone);
			}
		}
	}

	M_hash_strvp_destroy(win_alias, M_TRUE);
	M_hash_strvp_destroy(win_map, M_FALSE);
	M_list_str_destroy(zones_win);

	return tzs;
}
