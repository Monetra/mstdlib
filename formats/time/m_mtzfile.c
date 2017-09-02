#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse our extended Posix TZ DST rule.
 * The extended rule is "year;start[/time],end[/time]"
 * \param rule The rule to parse
 * \param adjust The adjustment object for the DST rule.
 * \param offset The offset that applies when DST is in effect.
 * \return Success on success. Otherwise error condition.
 */
static M_time_result_t M_mtzfile_tz_parse_dst_adjust_rule(const char *rule, M_time_tz_dst_rule_t **adjust, M_time_t offset, M_time_t offset_dst)
{
	char            **parts;
	M_parser_t       *parser;
	M_parser_t      **adjust_parts;
	size_t            num_parts;
	size_t            num_adjust_parts;
	M_int64           year;
	M_time_result_t   res;

	if (rule == NULL || *rule == '\0')
		return M_TIME_RESULT_INVALID;

	/* Split the year from the DST rule. */
	parts = M_str_explode_str(';', rule, &num_parts);
	if (parts == NULL || num_parts != 2 || !M_str_isnum(parts[0])) {
		M_str_explode_free(parts, num_parts);
		return M_TIME_RESULT_YEAR;
	}

	parser       = M_parser_create_const((const unsigned char *)parts[1], M_str_len(parts[1]), M_PARSER_FLAG_NONE);
	adjust_parts = M_parser_split(parser, ',', 0, M_PARSER_SPLIT_FLAG_NONE, &num_adjust_parts);
	if (adjust_parts == NULL || num_adjust_parts != 2) {
		M_parser_split_free(adjust_parts, num_adjust_parts);
		M_parser_destroy(parser);
		M_str_explode_free(parts, num_parts);
		return M_TIME_RESULT_INVALID;
	}

	year = M_str_to_int32(parts[0]);
	res  = M_time_tz_posix_parse_dst_adjust_rule(adjust_parts[0], adjust_parts[1], adjust, year, offset, offset_dst);

	M_parser_split_free(adjust_parts, num_adjust_parts);
	M_parser_destroy(parser);
	M_str_explode_free(parts, num_parts);
	return res;
}


static M_time_result_t M_mtzfile_tz_load_tz(M_time_tzs_t *tzs, M_ini_t *ini, const char *section, char **err_data)
{
	M_list_str_t         *aliases;
	M_list_str_t         *dsts;
	M_time_tz_rule_t     *rtz;
	M_parser_t           *parser;
	M_time_tz_dst_rule_t *adjust   = NULL;
	const char           *const_temp;
	char                 *full_key;
	M_time_t              offset_dst;
	size_t                len;
	size_t                i;
	M_time_result_t       res = M_TIME_RESULT_SUCCESS;

	if (err_data != NULL)
		*err_data = NULL;

	if (tzs == NULL || ini == NULL || section == NULL || *section == '\0') {
		return M_TIME_RESULT_INVALID;
	}

	rtz       = M_time_tz_rule_create();
	rtz->name = M_strdup(section);

	/* offset - Required */
	full_key   = M_ini_full_key(section, "offset");
	const_temp = M_ini_kv_get_direct(ini, full_key, 0);
	M_free(full_key);
	parser = M_parser_create_const((const unsigned char *)const_temp, M_str_len(const_temp), M_PARSER_FLAG_NONE);
	if (const_temp == NULL || *const_temp == '\0' || parser == NULL || M_parser_len(parser) == 0 || !M_time_tz_posix_parse_time_offset(parser, &(rtz->offset))) {
		M_parser_destroy(parser);
		M_time_tz_rule_destroy(rtz);
		if (err_data != NULL) {
			*err_data = M_strdup(const_temp);
		}
		return M_TIME_RESULT_OFFSET;
	}
	M_parser_destroy(parser);

	/* abbr - Required */
	full_key   = M_ini_full_key(section, "abbr");
	const_temp = M_ini_kv_get_direct(ini, full_key, 0);
	M_free(full_key);
	if (const_temp == NULL || *const_temp == '\0') {
		M_time_tz_rule_destroy(rtz);
		return M_TIME_RESULT_ABBR;
	}
	rtz->abbr = M_strdup(const_temp);

	/* abbr_dst - Optional */
	full_key   = M_ini_full_key(section, "abbr_dst");
	const_temp = M_ini_kv_get_direct(ini, full_key, 0);
	M_free(full_key);
	if (const_temp != NULL && *const_temp != '\0') {
		rtz->abbr_dst = M_strdup(const_temp);
	}

	/* offset_dst - Optional */
	offset_dst = 0;
	full_key   = M_ini_full_key(section, "offset_dst");
	const_temp = M_ini_kv_get_direct(ini, full_key, 0);
	M_free(full_key);
	parser = M_parser_create_const((const unsigned char *)const_temp, M_str_len(const_temp), M_PARSER_FLAG_NONE);
	if (const_temp != NULL && *const_temp != '\0' && parser != NULL && M_parser_len(parser) != 0 && !M_time_tz_posix_parse_time_offset(parser, &offset_dst)) {
		M_parser_destroy(parser);
		M_time_tz_rule_destroy(rtz);
		if (err_data != NULL) {
			*err_data = M_strdup(const_temp);
		}
		return M_TIME_RESULT_DSTOFFSET;
	}
	M_parser_destroy(parser);

	/* dst - Optional */
	full_key = M_ini_full_key(section, "dst");
	dsts     = M_ini_kv_get_vals(ini, full_key);
	M_free(full_key);
	if (dsts != NULL) {
		len = M_list_str_len(dsts);
		for (i=0; i<len; i++) {
			const_temp = M_list_str_at(dsts, i);
			res        = M_mtzfile_tz_parse_dst_adjust_rule(const_temp, &adjust, rtz->offset, offset_dst);
			if (res != M_TIME_RESULT_SUCCESS) {
				if (err_data != NULL) {
					*err_data = M_strdup(const_temp);
				}
				break;
			}
			M_time_tz_rule_add_dst_adjust(rtz, adjust);
		}
		M_list_str_destroy(dsts);
		if (res != M_TIME_RESULT_SUCCESS) {
			M_time_tz_rule_destroy(rtz);
			return res;
		}
	}

	/* alias - Optional */
	full_key = M_ini_full_key(section, "alias");
	aliases  = M_ini_kv_get_vals(ini, full_key);
	M_free(full_key);
	/* We want the name to be used as an alias. */
	if (aliases == NULL) {
		aliases = M_list_str_create(M_LIST_STR_NONE);
	}
	M_list_str_insert(aliases, section);

	/* Add the rtz to the tzs db */
	M_time_tz_rule_load(tzs, rtz, section, aliases);
	M_list_str_destroy(aliases);

	return M_TIME_RESULT_SUCCESS;
}

static M_time_result_t M_mtzfile_tzs_add_data(M_time_tzs_t *tzs, const char *data, M_bool data_is_str, size_t *err_line, char **err_sect, char **err_data)
{
	M_ini_t              *ini = NULL;
	M_ini_settings_t     *ini_settings;
	M_list_str_t         *sections;
	M_time_tzs_t         *tzs_temp;
	const char           *section;
	size_t                len;
	size_t                i;
	M_time_result_t       res = M_TIME_RESULT_SUCCESS;

	if (tzs == NULL || data == NULL || *data == '\0')
		return M_TIME_RESULT_INVALID;

	/* Setup how the ini is constructed. */
	ini_settings = M_ini_settings_create();
	M_ini_settings_set_element_delim_char(ini_settings, '\n');
	M_ini_settings_set_kv_delim_char(ini_settings, '=');
	M_ini_settings_set_comment_char(ini_settings, '#');
	M_ini_settings_set_quote_char(ini_settings, '"');
	M_ini_settings_set_escape_char(ini_settings, '\\');
	M_ini_settings_reader_set_dupkvs_handling(ini_settings, M_INI_DUPKVS_COLLECT);

	if (data_is_str) {
		ini = M_ini_read(data, ini_settings, M_FALSE, err_line);
	} else {
		ini = M_ini_read_file(data, ini_settings, M_FALSE, err_line, 12*1024*1024 /* 12 MB */);
	}
	if (ini == NULL) {
		M_ini_settings_destroy(ini_settings);
		return M_TIME_RESULT_INI;
	}
	M_ini_settings_destroy(ini_settings);

	/* We'll put all of the tz we load into a temporary tzs so we can determine if there are duplicates or not before
 	 * putting the data into the real tzs. */
	tzs_temp = M_time_tzs_create();

	/* Each section is a timezone. Loop though all sections and pull out the values that define it. */
	sections = M_ini_kv_sections(ini);
	len      = M_list_str_len(sections);
	for (i=0; i<len; i++) {
		section = M_list_str_at(sections, i);
		res     = M_mtzfile_tz_load_tz(tzs_temp, ini, section, err_data);
		if (res != M_TIME_RESULT_SUCCESS) {
			if (err_sect != NULL) {
				*err_sect = M_strdup(section);
			}
			break;
		}
	}

	M_list_str_destroy(sections);
	M_ini_destroy(ini);

	/* Merge our data from our temp tzs db into our real one. */
	if (res == M_TIME_RESULT_SUCCESS) {
		if (M_time_tzs_merge(&tzs, tzs_temp, err_sect)) {
			tzs_temp = NULL;
		} else {
			res = M_TIME_RESULT_DUP;
		}
	}
	M_time_tzs_destroy(tzs_temp);

	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_result_t M_mtzfile_tzs_add_str(M_time_tzs_t *tzs, const char *data, size_t *err_line, char **err_sect, char **err_data)
{
	return M_mtzfile_tzs_add_data(tzs, data, M_TRUE, err_line, err_sect, err_data);
}

M_time_result_t M_mtzfile_tzs_add_file(M_time_tzs_t *tzs, const char *path, size_t *err_line, char **err_sect, char **err_data)
{
	return M_mtzfile_tzs_add_data(tzs, path, M_FALSE, err_line, err_sect, err_data);
}
