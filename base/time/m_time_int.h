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

#ifndef __M_TIME_INT_H__
#define __M_TIME_INT_H__

#include "platform/m_platform.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! Lazy loading function prototype.
 * Used for lazy loading of timezone data.
 * \param name The name corresponding to the name key in M_time_tzs_t->tzs.
 * \param data Tz source specific data.
 * \param tz data.
 */
typedef M_time_tz_t *(*M_time_tzs_lazy_load_t)(const char *name, void *data);

#ifdef _WIN32
   typedef long M_time_tv_sec_t;
   typedef long M_time_tv_usec_t;
#else
   typedef time_t M_time_tv_sec_t;
#  if defined(_SCO_ELF) || defined(__SCO_VERSION__)
     typedef long M_time_tv_usec_t;
#  else
     typedef suseconds_t M_time_tv_usec_t;
#  endif  
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_TIME_TZ_RULE_OLSON = 1,  /*! Olson type rule */
	M_TIME_TZ_RULE_TZ    = 2   /*! Standard timezone rule */
} M_time_tz_rule_format_t;

/*! Timezone data.
 * The timezone data can be from one of a variety of timezone sources. This provides a standardized way to
 * work with data which is from different sources and internally is stored in different formats.
 *
 * To accomplish this the data is stored in the structure as a void pointer. Callbacks specific to the data
 * format are set in the structure for use with the data.
 */
struct M_time_tz {
	M_time_tz_rule_format_t type;
	void                   *data; /*!< The data in the source format. */
	/* Callbacks */
	void        (*destroy)(void *data); /*!< Destroy the void pointer data object. */
	M_time_t    (*adjust_fromlocal)(const void *data, const M_time_localtm_t *ltime); /*!< Get the amount of time a time in local time needs to be adjusted to UTC. */
	M_time_t    (*adjust_tolocal)(const void *data, M_time_t gmt, M_bool *isdst, const char **abbr); /*!< Get the amount of time a time in UTC needs to be adjusted for the timezone. */
};

/*! A list of DST rule adjustments. */
struct M_time_tz_dst_rules;
typedef struct M_time_tz_dst_rules M_time_tz_dst_rules_t;


/*! List of precomputed TZ/Zoneinfo/Olson database transitions. */
struct M_time_tz_olson_transitions;
typedef struct M_time_tz_olson_transitions M_time_tz_olson_transitions_t;


/*! A time zone using rules for determining DST. */
typedef struct {
	char                  *name;     /*!< Unique identifier for the timezone. Typically this will be a 3+
	                                      character timezone identifier. For example, EST5DST or EST5. */
	char                  *abbr;     /*<! The timezone abbreviation in use for standard time. */
	char                  *abbr_dst; /*<! The timezone abbreviation in use for DST time. */
	M_time_t               offset;   /*<! The timezone offset that the UTC time should be adjusted by. This is
	                                      only used if there are no adjustments. */
	M_time_tz_dst_rules_t *adjusts;  /*!< A list of DST adjustment rules. If a DST rule applies its offset
	                                      will be used instead of the above offset. */
} M_time_tz_rule_t;


/*! Represents the time and day a DST change takes place.
 * This is in local time (already adjusted for timezone offset).
 */
typedef struct {
	int month; /*!< Month. 1-12. Use 0 to specify that DST always applies. */
	int wday;  /*!< Day of week. 0=Sun ... 6=Sat. */
	int occur; /*!< The occurrence of the month the day falls on. E.g. 2 is the second occurrence of the month. -2 for
	                the second to last occurrence of the month. */
	int hour;  /*!< The hour of DST. 24 hr format. 0=midnight, 23=11pm */
	int min;   /*!< The minute of DST. 00-59. */
	int sec;   /*!< The second of DST. 00-59. */
} M_time_tz_dst_change_t;


/*! DST adjustment rule. */
typedef struct {
	M_int64                year;       /*<! The year the rule starts. */
	M_time_t               offset;     /*<! The timezone offset that the UTC time should be adjusted by. */
	M_time_t               offset_dst; /*<! The amount of time the timezone adjust time should be adjusted when
	                                        DST is in effect. This include the timezone offset and is not an
	                                        adjustment to the offset. */
	M_time_tz_dst_change_t start;      /*!< DST start. If start mon=0 than DST is always in effect for the
	                                        entire year. */
	M_time_tz_dst_change_t end;        /*!< DST end. */
} M_time_tz_dst_rule_t;


/*! Precomputed TZ/Zoneinfo/Olson database tranition. */
typedef struct {
	M_time_t    start;  /*!< Time in UTC when the transition takes effect. */
	M_time_t    offset; /*!< The offset that UTC needs to be adjusted by to be local time. */
	M_bool      isdst;  /*!< Is this a DST transition. */
	const char *abbr;   /*!< The abbreviation to use for this time. */
} M_time_tz_olson_transition_t;


/*! A mapping with olson, and Windows names with additional information about zone and if it's a main name. */
struct M_time_tz_info_map {
	const char        *olson_name;
	const char        *win_name;
	M_time_tz_zones_t  zone;
	M_bool             main;
};
typedef struct M_time_tz_info_map M_time_tz_info_map_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Windows helpers. */

#ifdef _WIN32
/*! Convert a Windows FILETIME to an int64.
 * \param[in]  ft The filetime.
 * \return 64 bit Integer.
 */
M_int64 M_time_filetime_to_int64(const FILETIME *ft);

/*! Convert an int64 to a Windows FILETIME.
 *
 * \warning Be careful because FILETIME is 100 nano second resultion.
 * Converting a M_time_t to a file time must use M_time_to_filetime.
 *
 * \param[in,out] ft The filetime.
 * \param[in]     v  64 bit Integer.
 *
 * \see M_time_to_filetime
 */
void M_time_filetime_from_int64(FILETIME *ft, M_int64 v);

/*! Convert a windows FILETIME to a M_timeval_t.
 * \param[in]  ft The filetime.
 * \param[out] tv The timeval to fill.
 */
void M_time_timeval_from_filetime(const FILETIME *ft, M_timeval_t *tv);

/*! Convert a windows FILETIME to a M_time_t.
 * \param[in] ft The filetime.
 * \return The time as a M_time_t;
 */
M_time_t M_time_from_filetime(const FILETIME *ft);

/*! Convert a M_time_t to a windows FILETIME.
 * \param[in]  t  The M_time_t to convert from.
 * \param[out] ft The filetime.
 * \return The time as a M_time_t;
 */
void M_time_to_filetime(M_time_t t, FILETIME *ft);

/*! Convert a M_time_t to a windows SYSTEMTIME.
 * \param[in]  t  The M_time_t to convert from.
 * \param[out] st The systemtime.
 * \return The time as a M_time_t;
 */
void M_time_to_systemtime(M_time_t t, SYSTEMTIME *st);
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * m_time.c */

M_API time_t M_time_M_time_t_to_time_t(M_time_t t);
M_API void M_time_M_timeval_t_to_struct_timeval(const M_timeval_t *mtv, struct timeval *stv, M_bool can_neg);

/* Time zone mapping. */
extern M_time_tz_info_map_t M_time_tz_zone_map[];



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * tz. */

/*! Destroy a tz object
 * \param tz The tz to destroy.
 */
M_API void M_time_tz_destroy(M_time_tz_t *tz);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * tzs. */

/*! Add a tz to the tzs db.
 * Takes ownership of tz.
 * \param tzs The tzs db.
 * \param tz The tz to add. Can be NULL (for lazy loading).
 * \param name The me to associate with the tz.
 * \return True on success. False if the tz could not be added. Typically this is because the name already has
 * a tz associated with it.
 */
M_API M_bool M_time_tzs_add_tz(M_time_tzs_t *tzs, M_time_tz_t *tz, const char *name);

/*! Add an alias to a tz name to the tzs db.
 * The name must already exist in the db to add an alias for the name.
 * If the alias already exists it till be replaced.
 * \param tzs The tzs db.
 * \param alias The alias to add.
 * \param name The name to associate with the alias.
 * \return True if the alias was successfully added. Otherwise false.
 */
M_API M_bool M_time_tzs_add_alias(M_time_tzs_t *tzs, const char *alias, const char *name);

/*! Merge two tzs dbs into one.
 * For a merge to succeed names in src cannot be in dest.
 * Alisas in src will overrite dest.
 * \param dest The tzs to merge into.
 * \param src The tzs to merge from. This will be destroyed on success.
 * \param err_name The name in src that is a duplicate if the return is due to a duplicate error.
 * \return True on success. Otherwise false.
 */
M_API M_bool M_time_tzs_merge(M_time_tzs_t **dest, M_time_tzs_t *src, char **err_name);

/*! Set the lazy load data for the db.
 * Only one source can be used for lazy loading.
 * \param tzs The tzs db.
 * \param func The lazy load function to call when loading data.
 * \param data Source specific data used for lazy loading.
 * \param data_destroy Function to destroy the source specific data called when the tzs is destroyed.
 */
M_API void M_time_tzs_set_lazy_load(M_time_tzs_t *tzs, M_time_tzs_lazy_load_t func, void *data, void (*data_destroy)(void *data));


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * tz rule based data. */

M_API M_time_tz_rule_t *M_time_tz_rule_create(void);
M_API void M_time_tz_rule_destroy(void *tz);
M_API M_bool M_time_tz_rule_add_dst_adjust(M_time_tz_rule_t *tz, M_time_tz_dst_rule_t *adjust);
M_API M_time_tz_t *M_time_tz_rule_create_tz(M_time_tz_rule_t *rtz);
M_API M_time_result_t M_time_tz_rule_load(M_time_tzs_t *tzs, M_time_tz_rule_t *rtz, const char *name, const M_list_str_t *aliases);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * tz dst rules (list) */

M_API M_time_tz_dst_rules_t *M_time_tz_dst_rules_create(void);
M_API void M_time_tz_dst_rules_destroy(M_time_tz_dst_rules_t *d);
M_API size_t M_time_tz_dst_rules_len(const M_time_tz_dst_rules_t *d);
M_API const M_time_tz_dst_rule_t *M_time_tz_dst_rules_at(const M_time_tz_dst_rules_t *d, size_t idx);
M_API M_bool M_time_tz_dst_rules_insert(M_time_tz_dst_rules_t *d, M_time_tz_dst_rule_t *val);
M_API M_bool M_time_tz_dst_rules_contains(const M_time_tz_dst_rules_t *d, M_int64 year);
/*! Get the DST rule for the given year.
 * \param d List.
 * \param year The year.
 * \return The rule >= year.
 */
M_API const M_time_tz_dst_rule_t *M_time_tz_dst_rules_get_rule(M_time_tz_dst_rules_t *d, M_int64 year);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Precomputed TZ/Zoneinfo/Olson database transitions (list) */

M_API M_time_tz_olson_transitions_t *M_time_tz_olson_transitions_create(void);
M_API void M_time_tz_olson_transitions_destroy(M_time_tz_olson_transitions_t *d);
M_API size_t M_time_tz_olson_transitions_len(M_time_tz_olson_transitions_t *d);
M_API const M_time_tz_olson_transition_t *M_time_tz_olson_transitions_at(M_time_tz_olson_transitions_t *d, size_t idx);
M_API M_bool M_time_tz_olson_transitions_insert(M_time_tz_olson_transitions_t *d, M_time_tz_olson_transition_t *val);
/*! Get the transition for the given UTC time.
 * \param d List
 * \param gmt Time.
 * \return Transition >= UTC.
 */
M_API const M_time_tz_olson_transition_t *M_time_tz_olson_transitions_get_transition(M_time_tz_olson_transitions_t *d, M_time_t gmt);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * POSIX timezone string parsing helpers. */

M_API M_bool M_time_tz_posix_parse_time_offset(M_parser_t *parser, M_time_t *offset);
M_API M_time_result_t M_time_tz_posix_parse_dst_adjust_rule(M_parser_t *parser_start, M_parser_t *parser_end, M_time_tz_dst_rule_t **adjust, M_int64 year, M_time_t offset, M_time_t offset_dst);

__END_DECLS

#endif /* __M_TIME_INT_H__ */
