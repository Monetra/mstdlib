/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#define M_TIME_TZ_OLSON_TZFILE_ID "TZif"
#define M_TIME_TZ_OLSON_1_DAY 86400

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! A timezone using the Olson/TZ/zoneinfo database. */
typedef struct {
	M_list_str_t                  *abbrs;       /*<! A list of abbreviations used by transitions. */
	M_time_tz_olson_transitions_t *transitions; /*!< A list of transitions. */
} M_time_tz_olson_t;

/*! Implementation of struct ttinfo as defined by man 5 tzfile.
 * This implementation changes some of the data types to make them more "correct" for this implementation.
 * isstd and isgmt is added to keep all data about a transition together.
 */
typedef struct {
	M_int64 tt_gmtoff;  /*!< Offset from UTC to get the local time. */
	M_bool  tt_isdst;   /*!< Is this transition a DST transition? */
	size_t  tt_abbrind; /*!< The location into the abbreviation array for this transitions abbreviation. */
#if 0
	/* We don't actually use these. They're supposed to aid when using the DB with Posix-style timezone
 	 * environment variables... I have no idea how they're actually used. Other systems that read tzfiles
	 * such as KDE's KTzfileTimeZoneSource and Pytz ignore these entirely. We're going to ignore them too. */
	M_bool  tt_isstd;   /*!< Is the transition time in standard or wall clock (dst?) time. */
	M_bool  tt_isgmt;   /*!< Is the transition time in in UTC or local time. */
#endif
} M_time_tz_olson_ttinfo_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an olson tz object.
 * \return A tz object for dealing with olson timezone database.
 */
static M_time_tz_olson_t *M_time_tz_olson_create(void)
{
	M_time_tz_olson_t *tz;

	tz              = M_malloc(sizeof(*tz));
	tz->abbrs       = M_list_str_create(M_LIST_STR_SORTASC);
	tz->transitions = M_time_tz_olson_transitions_create();

	return tz;
}

/*! Destroy an olson tz object.
 * \param tz The tz to destroy.
 */
static void M_time_tz_olson_destroy(void *tz)
{
	M_time_tz_olson_t *olson_tz;

	if (tz == NULL)
		return;

	olson_tz = (M_time_tz_olson_t *)tz;

	M_list_str_destroy(olson_tz->abbrs);
	olson_tz->abbrs = NULL;

	M_time_tz_olson_transitions_destroy(olson_tz->transitions);
	olson_tz->transitions = NULL;

	M_free(olson_tz);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a 32 or 64 bit value from a tzfile.
 * Some values in the tzfile can be 32 or 64 bits depending on the size of M_time_t.
 * The tzfile documentation has a number of values marked as "long". Typically a long in the docs
 * is 32 bits but in some cases it is stated to be 64 bits. A "long" in a tzfile is always stored in "standard"
 * (network) byte order.
 * \param fd The file we're reading from.
 * \param is64 Should we read 32 or 64 bits?
 * \return the value read.
 */
static M_int64 M_time_tz_olson_parse_tzfile_read_long(M_fs_file_t *fd, M_bool is64)
{
	unsigned char buf[8];
	size_t        read_len;
	M_uint32      val32;
	M_uint64      val64;
	size_t        len = 4;

	if (fd == NULL)
		return 0;

	if (is64)
		len = 8;

	M_mem_set(buf, 0, sizeof(buf));
	/* Read the appropriate amount of data into the buffer depending on if we need 64 bit data or not. */
	if (M_fs_file_read(fd, buf, len, &read_len, M_FS_FILE_RW_FULLBUF) != M_FS_ERROR_SUCCESS || read_len != len)
		return 0;

	if (is64) {
		M_mem_copy(&val64, buf, 8);
		return (M_int64)M_ntoh64(val64);
	}
	M_mem_copy(&val32, buf, 4);
	/* We have a cast to 32 followed by a cast to 64 because the value returned by M_ntoh32 is unsigned.
 	 * We need the wrapping for sign conversion to take place before the cast to the 64 bit value. An unsigned 32 bit
	 * value can fit into a signed 64 bit value. So without this explicit cast to signed 32 bit fist we won't get the
	 * correct value.
 	 */
	return (M_int64)((M_int32)M_ntoh32(val32));
}

/*! Read a single bye from a tzfile.
 * \param fd The file we're reading.
 * \return The value read.
 */
static unsigned char M_time_tz_olson_parse_tzfile_read_byte(M_fs_file_t *fd)
{
	unsigned char val;
	size_t        read_len;

	if (fd == NULL)
		return 0;

	if (M_fs_file_read(fd, &val, 1, &read_len, M_FS_FILE_RW_NORMAL) != M_FS_ERROR_SUCCESS)
		return 0;

	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Check that a given file starts with the tzfile identifying header.
 * \param fd The file we're reading.
 * \return True if the header is correct. Otherwise false.
 */
static M_bool M_time_tz_olson_parse_tzfile_check_header(M_fs_file_t *fd)
{
	char         buf[5];
	size_t       buf_len;
	size_t       read_len = 0;
	M_fs_error_t res      = M_FS_ERROR_SUCCESS;

	if (fd == NULL)
		return M_FALSE;

	buf_len = sizeof(buf);
	M_mem_set(buf, 0, buf_len);

	res = M_fs_file_read(fd, (unsigned char *)buf, buf_len-1, &read_len, M_FS_FILE_RW_FULLBUF);
	if (res != M_FS_ERROR_SUCCESS) {
		return M_FALSE;
	}

	if (!M_str_eq(M_TIME_TZ_OLSON_TZFILE_ID, buf)) {
		return M_FALSE;
	}

	return M_TRUE;
}

/*! Parse an olson tzfile into our olson tz object.
 * \param fd The file we're reading.
 * \param tz The olson tz object we will return.
 * \param skip_first Whether we can skip the first header if possible.
 * \param timet64 Should we read M_time_t values from the file as 64 values?
 * \return True if the file was successfully parsed and the tz was created. Otherwise false.
 */
static M_bool M_time_tz_olson_parse_tzfile(M_fs_file_t *fd, M_time_tz_olson_t **tz, M_bool skip_first, M_bool timet64)
{
	M_time_t                     *transition_times;
	unsigned char                *info_idxs;
	M_time_tz_olson_ttinfo_t     *ttinfos;
	M_time_tz_olson_transition_t *transition;
	char                         *abbrs;
	char                          ver;
	M_uint32                      ttisgmtcnt;
	M_uint32                      ttisstcnt;
	M_uint32                      leapcnt;
	M_uint32                      timecnt;
	M_uint32                      typecnt;
	M_uint32                      charcnt;
	size_t                        info_idx;
	size_t                        abbr_idx;
	size_t                        i;

	if (fd == NULL || tz == NULL)
		return M_FALSE;

	/* ID. */
	if (!M_time_tz_olson_parse_tzfile_check_header(fd))
		return M_FALSE;

	/* Version. */
	ver = (char)M_time_tz_olson_parse_tzfile_read_byte(fd);
	if (ver != '\0' && ver != '2')
		return M_FALSE;

	/* Reserved. */
	M_fs_file_seek(fd, 15, M_FS_FILE_SEEK_CUR);

	/* Section counts. */
	ttisgmtcnt = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
	ttisstcnt  = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
	leapcnt    = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
	timecnt    = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
	typecnt    = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
	charcnt    = (M_uint32)M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);

	/* These are all interrelated and should be the same length. The typecnt cannot be 0 according to the docs. */
	if (typecnt != ttisstcnt || typecnt != ttisgmtcnt || ttisstcnt != ttisgmtcnt || typecnt == 0)
		return M_FALSE;

	/* Check if we have a version 2 file which has 64 bit data. */
	if (skip_first && ver == '2') {
		/* The 64 bit data is a second complete version of the file after the 32 bit version. We skip past the
 		 * first set of data and start reading again at the location where the 64 bit data starts. */
		M_fs_file_seek(fd, ttisstcnt+ttisstcnt+(leapcnt*8)+(timecnt*5)+(typecnt*6)+charcnt, M_FS_FILE_SEEK_CUR);
		return M_time_tz_olson_parse_tzfile(fd, tz, M_FALSE, M_TRUE);
	}

	/* Create some places to store our data as we parse. */
	transition_times = M_malloc(sizeof(*transition_times)*timecnt);
	info_idxs        = M_malloc(sizeof(*info_idxs)*timecnt);
	ttinfos          = M_malloc(sizeof(*ttinfos)*typecnt);
	abbrs            = M_malloc(sizeof(*abbrs)*charcnt);

	/* Read the times and the index in the offset array. */
	for (i=0; i<timecnt; i++) {
		transition_times[i] = (M_time_t)M_time_tz_olson_parse_tzfile_read_long(fd, timet64);
	}
	for (i=0; i<timecnt; i++) {
		info_idxs[i] = M_time_tz_olson_parse_tzfile_read_byte(fd);
		/* Info index out of range. */
		if (info_idxs[i] > typecnt) {
			M_free(transition_times);
			M_free(info_idxs);
			M_free(ttinfos);
			M_free(abbrs);
			return M_FALSE;
		}
	}

	/* Read the offset array */
	for (i=0; i<typecnt; i++) {
		ttinfos[i].tt_gmtoff  = M_time_tz_olson_parse_tzfile_read_long(fd, M_FALSE);
		ttinfos[i].tt_isdst   = M_time_tz_olson_parse_tzfile_read_byte(fd);
		ttinfos[i].tt_abbrind = M_time_tz_olson_parse_tzfile_read_byte(fd);
		/* Abbreviation index out of range. */
		if (ttinfos[i].tt_abbrind > charcnt) {
			M_free(transition_times);
			M_free(info_idxs);
			M_free(ttinfos);
			M_free(abbrs);
			return M_FALSE;
		}
	}

	/* Read the abbreviations.
 	 * Note: The man 5 tzfile man page from the Linux man-pages project version 2012-05-04 does not
	 * document the location of the abbreviations properly. It leaves out this part of the format.
	 * The abbreviation data really is here.
	 * 
	 * The abbrs is a block of \0 separated strings.
	 * E.g: "LMT\0EDT\0EST\0EWT\0EPT"
	 *
	 * The tt_abbrind gives the start offest in the block where the abbr starts. The abbr ends when the first
	 * \0 is encountered. Meaning the abbr for tt_abbrind=4 is EDT in the above example.
	 */
	M_fs_file_read(fd, (unsigned char *)abbrs, charcnt, NULL, M_FS_FILE_RW_FULLBUF);

	/* Skip the leap seconds because we don't support them. */
	M_fs_file_seek(fd, (M_int64)leapcnt*(8+(timet64?4:0)), M_FS_FILE_SEEK_CUR);

	/* isstd and isgmt data. We're going to skip it because we don't use it. */
	M_fs_file_seek(fd, ttisstcnt+ttisgmtcnt, M_FS_FILE_SEEK_CUR);

	/* Now that we have all the data create the tz and fill it in. */
	*tz = M_time_tz_olson_create();
	for (i=0; i<timecnt; i++) {
		transition         = M_malloc(sizeof(*transition));
		info_idx           = info_idxs[i];

		transition->start  = transition_times[i];
		transition->offset = (M_time_t)ttinfos[info_idx].tt_gmtoff;
		transition->isdst  = ttinfos[info_idx].tt_isdst;

		/* Check if the abbreviation already exists or if we need to add it. */
		if (!M_list_str_index_of((*tz)->abbrs, abbrs+ttinfos[info_idx].tt_abbrind, M_LIST_STR_MATCH_VAL, &abbr_idx)) {
			M_list_str_insert((*tz)->abbrs, abbrs+ttinfos[info_idx].tt_abbrind);
			abbr_idx = M_list_str_len((*tz)->abbrs)-1;
		}
		transition->abbr   = M_list_str_at((*tz)->abbrs, abbr_idx);

		M_time_tz_olson_transitions_insert((*tz)->transitions, transition);
	}

	M_free(transition_times);
	M_free(info_idxs);
	M_free(ttinfos);
	M_free(abbrs);

	return M_TRUE;
}

/*! Parse a olson tz file at a given location.
 * \param path The file to parse.
 * \param tz The olson tz object we will return.
 * \return True on successful parse. Otherwise false.
 */
static M_bool M_time_tz_olson_parse_tzfile_path(const char *path, M_time_tz_olson_t **tz)
{
	M_fs_file_t  *fd;
	M_fs_error_t  res;

	if (path == NULL || *path == '\0' || tz == NULL) {
		return M_FALSE;
	}
	*tz = NULL;

	res = M_fs_file_open(&fd, path, M_FS_BUF_SIZE, M_FS_FILE_MODE_READ|M_FS_FILE_MODE_NOCREATE, NULL);
	if (res != M_FS_ERROR_SUCCESS) {
		return M_FALSE;
	}

	if (!M_time_tz_olson_parse_tzfile(fd, tz, M_TRUE, M_FALSE)) {
		M_fs_file_close(fd);
		return M_FALSE;
	}

	M_fs_file_close(fd);
	return M_TRUE;
}

/*! Get the adjust to a local time (in seconds) to convert to a UTC time.
 *
 * The Olson TZ database is in the form:
 * UTC transition time : adjustment to local.
 * The adjust to local time can include or excluded DST.
 *
 * Going from UTC to local time is simple. Take the UTC fine and find which transaction it corresponds to. The
 * transition it is after but before the next one. Transition 1 < UTC < transition 2. Transition 1 is the correct
 * transition.
 *
 * Going from local to UTC is more difficult for a few reasons. We have a time adjusted for local time and we have
 * a list of UTC times that correspond to an UTC adjustment. We are trying to figure out which adjustment is correct
 * when we only have an (unadjusted) local time.
 *
 * What we're going to do is adjust the local time forward by 1 day and backward by 1 day. We are then going to
 * determine which transitions correspond to these two times. 1 day works because it is more than the maximum
 * adjustment (including DST if it applies). It is also less than the minimum time between DST adjustments. While
 * this isn't perfect and future changes may cause issues in the assumptions used this currently works.
 *
 * With the two transitions we adjust the local time by each of the transaction times and check if they still apply.
 * If the transition is the same or one does not apply then we know we have the correct transition. This will be
 * the case most of the time.
 *
 * Due to DST we can run into a situation where a local time applies to two UTC times. 1:30 -> 2:00, at 2:00 fall
 * back to 1:00 -> 1:30 happens a second time. Everything from 1-2 happens twice in local time if the transition
 * time is 2:00 and the amount is 1 hour.
 *
 * If both transitions apply then we're going to use the one that is DST if the local time is known to be DST.
 * Otherwise we'll use the non-DST transition.
 *
 * Finally if neither (or both) transitions are DST we use the later one. This situation is possible. Apparently
 * in 1915 Europe/Warsaw switched to CET. This causes a time where two transitions apply equally. At this point
 * we're beyond the scope of what we want to accomplish/support with this library in regard to exact time conversion.
 */
static M_time_t M_time_tz_olson_adjust_fromlocal(const void *data, const M_time_localtm_t *ltime)
{
	const M_time_tz_olson_t            *olson_tz;
	const M_time_tz_olson_transition_t *transition_prev;
	const M_time_tz_olson_transition_t *transition_next;
	M_time_gmtm_t                       atime;
	M_time_t                            tstamp;
	M_bool                              prev_valid = M_FALSE;
	M_bool                              next_valid = M_FALSE;

	if (data == NULL || ltime == NULL)
		return 0;

	olson_tz = (const M_time_tz_olson_t *)data;

	/* ltime is const and M_time_fromgm is not because will modify its input. Hence the copy. */
	M_mem_copy(&atime, ltime, sizeof(atime));
	tstamp = M_time_fromgm(&atime);

	/* Get the two transitions 1 day before and 1 day after the local time. */
	transition_prev = M_time_tz_olson_transitions_get_transition(olson_tz->transitions, tstamp-M_TIME_TZ_OLSON_1_DAY);
	transition_next = M_time_tz_olson_transitions_get_transition(olson_tz->transitions, tstamp+M_TIME_TZ_OLSON_1_DAY);

	/* No transition was found... Can't get an offset from nothing. */
	if (transition_prev == NULL && transition_next == NULL)
		return 0;

	/* The transitions are the same. So there is only one that will work. */
	if (transition_prev == transition_next)
		return transition_prev->offset * -1;

	/* Figure out which of the two transitions are valid (if we have them). */
	if (transition_prev != NULL) {
		if (tstamp-transition_prev->offset >= transition_prev->start) {
			prev_valid = M_TRUE;
		}
	}
	if (transition_next != NULL) {
		if (tstamp-transition_next->offset >= transition_next->start) {
			next_valid = M_TRUE;
		}
	}

	/* Neither transition is valid. This shouldn't happen... */
	if (!prev_valid && !next_valid) {
		return 0;
	}
	/* Only one transition is valid so that's the right one. */
	if (prev_valid && !next_valid) {
		return transition_prev->offset * -1;
	}
	if (!prev_valid && next_valid) {
		return transition_next->offset * -1;
	}

	/* Both are valid use the one that is DST if DST is explicitly set and at least one has DST set. */
	if (transition_prev->isdst && !transition_next->isdst) {
		if (ltime->isdst == 1) {
			return transition_prev->offset * -1;
		} else {
			return transition_next->offset * -1;
		}
	}
	if (!transition_prev->isdst && transition_next->isdst) {
		if (ltime->isdst == 1) {
			return transition_next->offset * -1;
		} else {
			return transition_prev->offset * -1;
		}
	}

	/* Neither or both are DST so use next. This very special case is beyond what we want to support. */
	return transition_next->offset * -1;
}

/*! Get the adjustment to a UTC time that needs to be applied for the given timezone (including DST).
 * \param data The tz data to use for the adjustment.
 * \param time The UTC time that needs to be adjusted.
 * \param isdst True if DST is in effect. Otherwise false.
 * \param abbr The abbreviation used for this time.
 * \return The adjustment that needs to be applied to the UTC time.
 */
static M_time_t M_time_tz_olson_adjust_tolocal(const void *data, M_time_t gmt, M_bool *isdst, const char **abbr)
{
	const M_time_tz_olson_t            *olson_tz;
	const M_time_tz_olson_transition_t *transition;

	if (data == NULL)
		return 0;
	if (isdst)
		*isdst = M_FALSE;
	if (abbr)
		*abbr = NULL;

	olson_tz = (const M_time_tz_olson_t *)data;

	transition = M_time_tz_olson_transitions_get_transition(olson_tz->transitions, gmt);
	if (transition == NULL)
		return 0;

	if (isdst)
		*isdst = transition->isdst;
	if (abbr)
		*abbr = transition->abbr;
	return transition->offset;
}

/*! Load an olson tz file.
 * \param path The file to load.
 * \return A tz object.
 */
static M_time_tz_t *M_time_tz_olson_load_tzfile(const char *path)
{
	M_time_tz_t       *tz;
	M_time_tz_olson_t *olson_tz;

	if (path == NULL || *path == '\0')
		return NULL;

	if (!M_time_tz_olson_parse_tzfile_path(path, &olson_tz))
		return NULL;

	/* Create the tz to put the data in. */
	tz                   = M_malloc(sizeof(*tz));
	tz->data             = olson_tz;
	tz->destroy          = M_time_tz_olson_destroy;
	tz->adjust_fromlocal = M_time_tz_olson_adjust_fromlocal;
	tz->adjust_tolocal   = M_time_tz_olson_adjust_tolocal;

	return tz;
}

/*! Lazy load an olson tz file. */
static M_time_tz_t *M_time_tz_olson_lazy_load_tzfile(const char *name, void *data)
{
	const char *path;

	/* Check that we have a base path. */
	path = (const char *)data;
	if (path == NULL)
		return NULL;

	/* Check that what we're trying to load is a path under our base path. */
	if (!M_str_eq_max(name, path, M_str_len(path)))
		return NULL;

	return M_time_tz_olson_load_tzfile(name);
}

/*! Load a specific zone from an olson, tz, zoneinfo database.
 * \param tzs Our tz database object we will return.
 * \param path The path to the zoneinfo dir.
 * \param zone The specific zone (dir) to load.
 * \param lazy Should we use lazy loading. True if we should only load aliases now. False to load aliases and data.
 */
static void M_time_tz_olson_load_zone(M_time_tzs_t *tzs, const char *path, const char *zone, M_uint32 alias_f, M_uint32 flags)
{
	M_fs_dir_entries_t     *entries;
	M_time_tz_t            *tz         = NULL;
	M_time_tz_info_map_t   *map_entry;
	const M_fs_dir_entry_t *entry;
	const char             *entry_name;
	const char             *resolved_name;
	char                   *name;
	char                   *dreal_name;
	char                   *real_name;
	char                   *olson_name;
	char                   *full_path;
	size_t                  path_len;
	size_t                  len;
	size_t                  i;
	size_t                  j;
	M_fs_error_t            res;

	full_path = M_fs_path_join(path, zone, M_FS_SYSTEM_AUTO);
	entries   = M_fs_dir_walk_entries(full_path, NULL,
		M_FS_DIR_WALK_FILTER_FILE|M_FS_DIR_WALK_FILTER_SYMLINK|M_FS_DIR_WALK_FILTER_RECURSE|M_FS_DIR_WALK_FILTER_READ_INFO_BASIC);

	path_len = M_str_len(path);
	len      = M_fs_dir_entries_len(entries);
	for (i=0; i<len; i++) {
		entry = M_fs_dir_entries_at(entries, i);

		entry_name = M_fs_dir_entry_get_name(entry);
		if (entry_name == NULL || *entry_name == '\0') {
			continue;
		}
		name       = M_fs_path_join(zone, entry_name, M_FS_SYSTEM_AUTO);
		olson_name = M_fs_path_join(zone, entry_name, M_FS_SYSTEM_UNIX);

		/* We need to resolve the symlink to what it points to. We don't want to load the same data multiple times. */
		if (M_fs_dir_entry_get_type(entry) == M_FS_TYPE_SYMLINK) {
			resolved_name = M_fs_dir_entry_get_resolved_name(entry);
			if (resolved_name == NULL || *resolved_name == '\0') {
				M_free(name);
				continue;
			}

			dreal_name = M_fs_path_join_resolved(full_path, entry_name, resolved_name, M_FS_SYSTEM_AUTO);
			/* Ensure the path is a real path and turn it into an absolute path. This way we always know if
 			 * to locations really point to the same file. */
			res = M_fs_path_norm(&real_name, dreal_name, M_FS_PATH_NORM_ABSOLUTE|M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
			M_free(dreal_name);
			if (res != M_FS_ERROR_SUCCESS) {
				M_free(name);
				continue;
			}
		} else {
			real_name  = M_fs_path_join(path, name, M_FS_SYSTEM_AUTO);
		}

		/* Check that what we're trying to load is a path under our base path. */
		if (!M_str_eq_max(real_name, path, path_len)) {
			M_free(real_name);
			M_free(olson_name);
			M_free(name);
			continue;
		}

		map_entry = NULL;
		if (alias_f == M_TIME_TZ_ALIAS_ALL || alias_f & (M_TIME_TZ_ALIAS_OLSON_MAIN|M_TIME_TZ_ALIAS_WINDOWS_ALL|M_TIME_TZ_ALIAS_WINDOWS_MAIN)) {
			for (j=0; M_time_tz_zone_map[j].olson_name!=NULL; j++) {
				if (M_str_caseeq(olson_name, M_time_tz_zone_map[j].olson_name)) {
					map_entry = &M_time_tz_zone_map[j];
					break;
				}
			}
		}

		/* If it's not in our mapping and we require a mapping then we're going to ingore it. */
		if (map_entry == NULL && alias_f != M_TIME_TZ_ALIAS_ALL && !(alias_f & (M_TIME_TZ_ALIAS_OLSON_ALL))) {
			M_free(real_name);
			M_free(olson_name);
			M_free(name);
			continue;
		}

		/* Add zone. */
		if (flags & M_TIME_TZ_LOAD_LAZY) {
			M_time_tzs_add_tz(tzs, NULL, real_name);
		} else {
			/* Check if the tz was already loaded. If we're not doing lazy loading the lazy function won't be
 			 * set so we can safely load this ourselves here. */
			if (M_time_tzs_get_tz(tzs, name) == NULL) {
				/* Load the tz. */
				tz = M_time_tz_olson_load_tzfile(real_name);
				if (tz != NULL && !M_time_tzs_add_tz(tzs, tz, real_name)) {
					M_time_tz_destroy(tz);
					continue;
				}
			}
		}

		/* Add alias. */
		if (map_entry == NULL || alias_f == M_TIME_TZ_ALIAS_ALL || alias_f & M_TIME_TZ_ALIAS_OLSON_ALL || (map_entry != NULL && alias_f & M_TIME_TZ_ALIAS_OLSON_MAIN && map_entry->main)) {
			M_time_tzs_add_alias(tzs, olson_name, real_name);
		}
		/* Windows aliases are duplicated so we're only going to load the aliases for main names this way we only
 		 * load one Windows name and don't have the same one pointing to multiple zones. */
		if (map_entry != NULL && map_entry->main && (alias_f == M_TIME_TZ_ALIAS_ALL || (alias_f & (M_TIME_TZ_ALIAS_WINDOWS_ALL|M_TIME_TZ_ALIAS_WINDOWS_MAIN)))) {
			M_time_tzs_add_alias(tzs, map_entry->win_name, real_name);
		}

		M_free(real_name);
		M_free(olson_name);
		M_free(name);
	}

	M_fs_dir_entries_destroy(entries);
	M_free(full_path);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_result_t M_time_tzs_add_tzfile(M_time_tzs_t *tzs, const char *path, const char *name)
{
	M_time_tz_t *tz;

	if (tzs == NULL || path == NULL || *path == '\0' || name == NULL || *name == '\0')
		return M_TIME_RESULT_INVALID;

	tz = M_time_tz_olson_load_tzfile(path);
	if (tz == NULL)
		return M_TIME_RESULT_ERROR;

	if (!M_time_tzs_add_tz(tzs, tz, name)) {
		M_time_tz_destroy(tz);
		return M_TIME_RESULT_DUP;
	}
	M_time_tzs_add_alias(tzs, name, name);

	return M_TIME_RESULT_SUCCESS;
}

M_time_tzs_t *M_time_tzs_load_zoneinfo(const char *path, M_uint32 zones, M_uint32 alias_f, M_uint32 flags)
{
	M_time_tzs_t *tzs       = NULL;
	char         *norm_path;
	M_fs_error_t  res;
	size_t        i;
	struct {
		const char *path;
	} sys_paths[] = {
		{ "/usr/share/zoneinfo" },
		{ "/usr/lib/zoneinfo"   },
		{ NULL }
	};

	if (M_str_isempty(path)) {
#ifdef _WIN32
		(void)info;
		/* The zoneinfo isn't standard on Windows so if we're not told where it is we can't load any data. */
		return NULL;
#else
		for (i=0; sys_paths[i].path!=NULL; i++) {
			if (M_fs_perms_can_access(sys_paths[i].path, M_FS_FILE_MODE_READ) == M_FS_ERROR_SUCCESS) {
				path = sys_paths[i].path;
				break;
			}
		}
		if (M_str_isempty(path)) {
			return NULL;
		}
#endif
	}

	/* Normalize the path and make it an absolute path. We don't know what's going to happen with the cwd
 	 * so we want to ensure we can always read the data. This is especially necessary for lazy loading. */
	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_ABSOLUTE|M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res !=  M_FS_ERROR_SUCCESS) {
		return NULL;
	}

	tzs = M_time_tzs_create();
	if (flags & M_TIME_TZ_LOAD_LAZY) {
		/* Store the base path so we can be sure anything lazy loaded is really under the path. */
		M_time_tzs_set_lazy_load(tzs, M_time_tz_olson_lazy_load_tzfile, norm_path, M_free);
	}

	/* Load the data for each zone that is requested. */
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_AFRICA) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Africa", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_AMERICA) {
		M_time_tz_olson_load_zone(tzs, norm_path, "America", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_ANTARCTICA) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Antarctica", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_ARCTIC) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Arctic", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_ASIA) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Asia", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_ATLANTIC) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Atlantic", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_AUSTRALIA) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Australia", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_EUROPE) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Europe", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_INDIAN) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Indian", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_PACIFIC) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Pacific", alias_f, flags);
	}
	if (zones == M_TIME_TZ_ZONE_ALL || zones & M_TIME_TZ_ZONE_ETC) {
		M_time_tz_olson_load_zone(tzs, norm_path, "Etc", alias_f, flags);
	}

	if (!(flags & M_TIME_TZ_LOAD_LAZY))
		M_free(norm_path);
	return tzs;
}
