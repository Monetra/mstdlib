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
#include "conf/m_conf_int.h"


/* --- Helper functions --- */

/* Decrease the number of values marked as available for this key. */
static void conf_decrement_key(M_conf_t *conf, const char *key, M_bool set_to_zero)
{
	M_uint64 num;

	if (conf != NULL && conf->unused_keys != NULL && !M_str_isempty(key)) {
		if (set_to_zero) {
			/* The caller wants to marks all instances of this key as used. */
			num = 0;
		} else {
			num = M_hash_stru64_get_direct(conf->unused_keys, key);
			if (num > 0) {
				num--;
			}
		}

		if (num == 0) {
			/* We've used up all instances of this key and can remove it from the table now. */
			M_hash_stru64_remove(conf->unused_keys, key);
		} else {
			/* There are more instances of this key left. */
			M_hash_stru64_insert(conf->unused_keys, key, num);
		}
	}
}

/* Build the settings we need for reading in ini files. */
static M_ini_settings_t *conf_build_ini_settings(M_bool allow_multiple)
{
	M_ini_settings_t *ini_settings;

	/* Establish the ini settings to use for building ini objects. */
	ini_settings = M_ini_settings_create();

	/* Set some special characters. */
	M_ini_settings_set_element_delim_char(ini_settings, '\n');
	M_ini_settings_set_quote_char(ini_settings, '"');
	M_ini_settings_set_escape_char(ini_settings, '\\');
	M_ini_settings_set_comment_char(ini_settings, '#');
	M_ini_settings_set_kv_delim_char(ini_settings, '=');

	/* Probably not necessary, but we'll set just in case. */
	M_ini_settings_set_padding(ini_settings, M_INI_PADDING_AFTER_COMMENT_CHAR);

	if (allow_multiple) {
		/* Allow one key to have multiple values. */
		M_ini_settings_reader_set_dupkvs_handling(ini_settings, M_INI_DUPKVS_COLLECT);
	} else {
		/* Only the last key is honored. */
		M_ini_settings_reader_set_dupkvs_handling(ini_settings, M_INI_DUPKVS_REMOVE_PREV);
	}

	return ini_settings;
}

/* Log a message with the provided loggers. */
static void conf_log_msg(M_conf_t *conf, M_list_t *loggers, const char *fmt, va_list ap)
{
	char            *msg;
	size_t           num_loggers;
	unsigned int     i;
	M_conf_logger_t  logger;

	if (conf == NULL || loggers == NULL || M_str_isempty(fmt))
		return;

	M_vasprintf(&msg, fmt, ap);

	num_loggers = M_list_len(loggers);
	for (i=0; i<num_loggers; i++) {
		logger = M_list_at(loggers, i);
		logger(conf->ini_path, msg);
	}

	M_free(msg);
}

/* Log a debug message for all registered debug loggers. */
static void conf_log_debug(M_conf_t *conf, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	conf_log_msg(conf, conf->debug_loggers, fmt, ap);
	va_end(ap);
}

/* Log an error message for all registered error loggers. */
static void conf_log_error(M_conf_t *conf, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	conf_log_msg(conf, conf->error_loggers, fmt, ap);
	va_end(ap);
}


/* --- Registration objects --- */

/* Create a registration object. */
static M_conf_reg_t *reg_create(const char *key, M_conf_reg_type_t type)
{
	M_conf_reg_t *reg;

	reg = M_malloc_zero(sizeof(*reg));

	reg->key  = M_strdup(key);
	reg->type = type;

	return reg;
}

/* Destroy a registration object. */
static void reg_destroy(M_conf_reg_t *reg)
{
	if (reg != NULL) {
		switch (reg->type) {
			case M_CONF_REG_TYPE_BUF:
				M_free(reg->default_val.buf);
				M_free(reg->regex);
				break;
			case M_CONF_REG_TYPE_STRDUP:
				M_free(reg->default_val.strdup);
				M_free(reg->regex);
				break;
			default:
				break;
		}

		M_free(reg->key);
		M_free(reg);
	}
}

/* Zero out the memory for this registration. */
static void reg_zero(M_conf_reg_t *reg)
{
	switch (reg->type) {
		case M_CONF_REG_TYPE_BUF:
			M_mem_set(reg->mem.buf, 0, reg->buf_len);
			break;
		case M_CONF_REG_TYPE_STRDUP:
			*(reg->mem.strdup) = NULL;
			break;
		case M_CONF_REG_TYPE_INT8:
			*(reg->mem.int8) = 0;
			break;
		case M_CONF_REG_TYPE_INT16:
			*(reg->mem.int16) = 0;
			break;
		case M_CONF_REG_TYPE_INT32:
			*(reg->mem.int32) = 0;
			break;
		case M_CONF_REG_TYPE_INT64:
			*(reg->mem.int64) = 0;
			break;
		case M_CONF_REG_TYPE_UINT8:
			*(reg->mem.uint8) = 0;
			break;
		case M_CONF_REG_TYPE_UINT16:
			*(reg->mem.uint16) = 0;
			break;
		case M_CONF_REG_TYPE_UINT32:
			*(reg->mem.uint32) = 0;
			break;
		case M_CONF_REG_TYPE_UINT64:
			*(reg->mem.uint64) = 0;
			break;
		case M_CONF_REG_TYPE_BOOL:
			*(reg->mem.boolean) = M_FALSE;
			break;
		case M_CONF_REG_TYPE_CUSTOM:
			break;
		default:
			break;
	}
}

/* Check if a registration was set with a converter callback or not. */
static M_bool reg_has_converter(M_conf_reg_t *reg)
{
	switch (reg->type) {
		case M_CONF_REG_TYPE_BUF:
			return reg->converter.buf != NULL;
		case M_CONF_REG_TYPE_STRDUP:
			return reg->converter.strdup != NULL;
		case M_CONF_REG_TYPE_INT8:
			return reg->converter.int8 != NULL;
		case M_CONF_REG_TYPE_INT16:
			return reg->converter.int16 != NULL;
		case M_CONF_REG_TYPE_INT32:
			return reg->converter.int32 != NULL;
		case M_CONF_REG_TYPE_INT64:
			return reg->converter.int64 != NULL;
		case M_CONF_REG_TYPE_UINT8:
			return reg->converter.uint8 != NULL;
		case M_CONF_REG_TYPE_UINT16:
			return reg->converter.uint16 != NULL;
		case M_CONF_REG_TYPE_UINT32:
			return reg->converter.uint32 != NULL;
		case M_CONF_REG_TYPE_UINT64:
			return reg->converter.uint64 != NULL;
		case M_CONF_REG_TYPE_BOOL:
			return reg->converter.boolean != NULL;
		case M_CONF_REG_TYPE_CUSTOM:
			return reg->converter.custom != NULL;
		default:
			return M_FALSE;
	}
}

/* Call the custom callback for this registration. */
static M_bool reg_call_converter(M_conf_reg_t *reg, const char *value)
{
	switch (reg->type) {
		case M_CONF_REG_TYPE_BUF:
			return reg->converter.buf(reg->mem.buf, reg->buf_len, value, reg->default_val.buf);
		case M_CONF_REG_TYPE_STRDUP:
			return reg->converter.strdup(reg->mem.strdup, value, reg->default_val.strdup);
		case M_CONF_REG_TYPE_INT8:
			return reg->converter.int8(reg->mem.int8, value, reg->default_val.int8);
		case M_CONF_REG_TYPE_INT16:
			return reg->converter.int16(reg->mem.int16, value, reg->default_val.int16);
		case M_CONF_REG_TYPE_INT32:
			return reg->converter.int32(reg->mem.int32, value, reg->default_val.int32);
		case M_CONF_REG_TYPE_INT64:
			return reg->converter.int64(reg->mem.int64, value, reg->default_val.int64);
		case M_CONF_REG_TYPE_UINT8:
			return reg->converter.uint8(reg->mem.uint8, value, reg->default_val.uint8);
		case M_CONF_REG_TYPE_UINT16:
			return reg->converter.uint16(reg->mem.uint16, value, reg->default_val.uint16);
		case M_CONF_REG_TYPE_UINT32:
			return reg->converter.uint32(reg->mem.uint32, value, reg->default_val.uint32);
		case M_CONF_REG_TYPE_UINT64:
			return reg->converter.uint64(reg->mem.uint64, value, reg->default_val.uint64);
		case M_CONF_REG_TYPE_BOOL:
			return reg->converter.boolean(reg->mem.boolean, value, reg->default_val.boolean);
		case M_CONF_REG_TYPE_CUSTOM:
			return reg->converter.custom(value);
		default:
			return M_FALSE;
	}
}

/* Validate the value as a string. */
static M_bool reg_validate_value_str(M_conf_t *conf, M_conf_reg_t *reg, const char *value, char *err_buf, size_t err_len)
{
	M_bool  ret = M_FALSE;
	M_re_t *regex;

	if (M_str_isempty(reg->regex)) {
		ret = M_TRUE;
		conf_log_debug(conf, "  Skipping regular expression check");
	} else {
		regex = M_re_compile(reg->regex, M_RE_CASECMP);
		if (regex == NULL) {
			M_snprintf(err_buf, err_len, "Invalid regex");
		} else {
			ret = M_re_eq(regex, value);
			if (ret) {
				conf_log_debug(conf, "  Passed regex check");
			} else {
				M_snprintf(err_buf, err_len, "Regex check failed");
			}
		}
		M_re_destroy(regex);
	}

	return ret;
}

/* Validate the value as a signed integer. */
static M_bool reg_validate_value_int(M_conf_t *conf, M_conf_reg_t *reg, const char *value, char *err_buf, size_t err_len)
{
	M_int64 num;
	M_int64 min_allowed  = 0;
	M_int64 max_allowed  = 0;
	M_int64 min_possible = 0;
	M_int64 max_possible = 0;

	(void)conf;

	if (!M_str_isnum(value)) {
		M_snprintf(err_buf, err_len, "Not a number");
		return M_FALSE;
	}

	num = M_str_to_int64(value);

	switch (reg->type) {
		case M_CONF_REG_TYPE_INT8:
			min_allowed  = reg->min_sval;
			max_allowed  = reg->max_sval;
			min_possible = M_INT8_MIN;
			max_possible = M_INT8_MAX;
			break;
		case M_CONF_REG_TYPE_INT16:
			min_allowed  = reg->min_sval;
			max_allowed  = reg->max_sval;
			min_possible = M_INT16_MIN;
			max_possible = M_INT16_MAX;
			break;
		case M_CONF_REG_TYPE_INT32:
			min_allowed  = reg->min_sval;
			max_allowed  = reg->max_sval;
			min_possible = M_INT32_MIN;
			max_possible = M_INT32_MAX;
			break;
		case M_CONF_REG_TYPE_INT64:
			min_allowed  = reg->min_sval;
			max_allowed  = reg->max_sval;
			min_possible = M_INT64_MIN;
			max_possible = M_INT64_MAX;
			break;
		default:
			break;
	}

	if (
		(min_allowed > min_possible && num < min_allowed) ||
		(max_allowed < max_possible && num > max_allowed) ||
		(num < min_possible) || (num > max_possible)
	) {
		M_snprintf(err_buf, err_len, "Value outside of allowed bounds");
		return M_FALSE;
	}

	return M_TRUE;
}

/* Validate the value as an unsigned integer. */
static M_bool reg_validate_value_uint(M_conf_t *conf, M_conf_reg_t *reg, const char *value, char *err_buf, size_t err_len)
{
	M_uint64 num;
	M_uint64 min_allowed  = 0;
	M_uint64 max_allowed  = 0;
	M_uint64 min_possible = 0;
	M_uint64 max_possible = 0;

	(void)conf;

	if (!M_str_isnum(value)) {
		M_snprintf(err_buf, err_len, "Not a number");
		return M_FALSE;
	}

	/* Let's also make sure that the value is not negative. */
	if (M_str_to_int64(value) < 0) {
		M_snprintf(err_buf, err_len, "Negative value not allowed");
		return M_FALSE;
	}

	num = M_str_to_uint64(value);

	switch (reg->type) {
		case M_CONF_REG_TYPE_UINT8:
			min_allowed  = reg->min_uval;
			max_allowed  = reg->max_uval;
			min_possible = 0;
			max_possible = M_UINT8_MAX;
			break;
		case M_CONF_REG_TYPE_UINT16:
			min_allowed  = reg->min_uval;
			max_allowed  = reg->max_uval;
			min_possible = 0;
			max_possible = M_UINT16_MAX;
			break;
		case M_CONF_REG_TYPE_UINT32:
			min_allowed  = reg->min_uval;
			max_allowed  = reg->max_uval;
			min_possible = 0;
			max_possible = M_UINT32_MAX;
			break;
		case M_CONF_REG_TYPE_UINT64:
			min_allowed  = reg->min_uval;
			max_allowed  = reg->max_uval;
			min_possible = 0;
			max_possible = M_UINT64_MAX;
			break;
		default:
			return M_FALSE;
	}

	if (
		(min_allowed > min_possible && num < min_allowed) ||
		(max_allowed < max_possible && num > max_allowed) ||
		(num > max_possible)
	) {
		M_snprintf(err_buf, err_len, "Value outside of allowed bounds");
		return M_FALSE;
	}

	return M_TRUE;
}

/* Run through any validators set for this registration. */
static M_bool reg_validate_value(M_conf_t *conf, M_conf_reg_t *reg, const char *value, char *err_buf, size_t err_len)
{
	switch (reg->type) {
		case M_CONF_REG_TYPE_BUF:
		case M_CONF_REG_TYPE_STRDUP:
			return reg_validate_value_str(conf, reg, value, err_buf, err_len);
		case M_CONF_REG_TYPE_INT8:
		case M_CONF_REG_TYPE_INT16:
		case M_CONF_REG_TYPE_INT32:
		case M_CONF_REG_TYPE_INT64:
			return reg_validate_value_int(conf, reg, value, err_buf, err_len);
		case M_CONF_REG_TYPE_UINT8:
		case M_CONF_REG_TYPE_UINT16:
		case M_CONF_REG_TYPE_UINT32:
		case M_CONF_REG_TYPE_UINT64:
			return reg_validate_value_uint(conf, reg, value, err_buf, err_len);
		default:
			return M_TRUE;
	}
}

/* Set the value for this registration. */
static void reg_set_value(M_conf_t *conf, M_conf_reg_t *reg, const char *value)
{
	switch (reg->type) {
		case M_CONF_REG_TYPE_BUF:
			if (M_str_isempty(value)) {
				M_str_cpy(reg->mem.buf, reg->buf_len, reg->default_val.buf);
			} else {
				M_str_cpy(reg->mem.buf, reg->buf_len, value);
			}
			conf_log_debug(conf, "  Setting %s: %s", reg->key, reg->mem.buf);
			break;
		case M_CONF_REG_TYPE_STRDUP:
			if (M_str_isempty(value)) {
				*(reg->mem.strdup) = M_strdup(reg->default_val.strdup);
			} else {
				*(reg->mem.strdup) = M_strdup(value);
			}
			conf_log_debug(conf, "  Setting %s: %s", reg->key, *(reg->mem.strdup));
			break;
		case M_CONF_REG_TYPE_INT8:
			if (M_str_isempty(value)) {
				*(reg->mem.int8) = reg->default_val.int8;
			} else {
				*(reg->mem.int8) = (M_int8)M_str_to_int64(value);
			}
			conf_log_debug(conf, "  Setting %s: %d", reg->key, *(reg->mem.int8));
			break;
		case M_CONF_REG_TYPE_INT16:
			if (M_str_isempty(value)) {
				*(reg->mem.int16) = reg->default_val.int16;
			} else {
				*(reg->mem.int16) = (M_int16)M_str_to_int64(value);
			}
			conf_log_debug(conf, "  Setting %s: %d", reg->key, *(reg->mem.int16));
			break;
		case M_CONF_REG_TYPE_INT32:
			if (M_str_isempty(value)) {
				*(reg->mem.int32) = reg->default_val.int32;
			} else {
				*(reg->mem.int32) = (M_int32)M_str_to_int64(value);
			}
			conf_log_debug(conf, "  Setting %s: %d", reg->key, *(reg->mem.int32));
			break;
		case M_CONF_REG_TYPE_INT64:
			if (M_str_isempty(value)) {
				*(reg->mem.int64) = reg->default_val.int64;
			} else {
				*(reg->mem.int64) = M_str_to_int64(value);
			}
			conf_log_debug(conf, "  Setting %s: %lld", reg->key, *(reg->mem.int64));
			break;
		case M_CONF_REG_TYPE_UINT8:
			if (M_str_isempty(value)) {
				*(reg->mem.uint8) = reg->default_val.uint8;
			} else {
				*(reg->mem.uint8) = (M_uint8)M_str_to_uint64(value);
			}
			conf_log_debug(conf, "  Setting %s: %u", reg->key, *(reg->mem.uint8));
			break;
		case M_CONF_REG_TYPE_UINT16:
			if (M_str_isempty(value)) {
				*(reg->mem.uint16) = reg->default_val.uint16;
			} else {
				*(reg->mem.uint16) = (M_uint16)M_str_to_uint64(value);
			}
			conf_log_debug(conf, "  Setting %s: %u", reg->key, *(reg->mem.uint16));
			break;
		case M_CONF_REG_TYPE_UINT32:
			if (M_str_isempty(value)) {
				*(reg->mem.uint32) = reg->default_val.uint32;
			} else {
				*(reg->mem.uint32) = (M_uint32)M_str_to_uint64(value);
			}
			conf_log_debug(conf, "  Setting %s: %u", reg->key, *(reg->mem.uint32));
			break;
		case M_CONF_REG_TYPE_UINT64:
			if (M_str_isempty(value)) {
				*(reg->mem.uint64) = reg->default_val.uint64;
			} else {
				*(reg->mem.uint64) = M_str_to_uint64(value);
			}
			conf_log_debug(conf, "  Setting %s: %llu", reg->key, *(reg->mem.uint64));
			break;
		case M_CONF_REG_TYPE_BOOL:
			if (M_str_isempty(value)) {
				*(reg->mem.boolean) = reg->default_val.boolean;
			} else {
				*(reg->mem.boolean) = M_str_istrue(value);
			}
			conf_log_debug(conf, "  Setting %s: %s", reg->key, *(reg->mem.boolean) ? "true" : "false");
			break;
		default:
			break;
	}
}

/* Set the value for this registration. */
static M_bool reg_handle(M_conf_t *conf, M_conf_reg_t *reg)
{
	const char *value;
	M_bool      ret;
	char        err_buf[256];

	if (reg == NULL)
		return M_FALSE;

	/* Set the zero value of this registration. */
	reg_zero(reg);

	/* Get the value for this registration's key. */
	value = M_conf_get_value(conf, reg->key);
	conf_log_debug(conf, "Parsing key: %s", reg->key);
	conf_log_debug(conf, "  Value in config file: %s", value);

	/* If this registration has a custom callback set, then we want to let that do all the work for validating and
 	 * setting the value. M_CONF_REG_TYPE_CUSTOM registrations always have a converter callback set. */
	if (reg_has_converter(reg)) {
		ret = reg_call_converter(reg, value);
		if (ret) {
			conf_log_debug(conf, "  Value manually set");
		} else {
			conf_log_error(conf, "Key '%s' failed manual conversion for value '%s'", reg->key, value);
		}
		return ret;
	}

	/* If there are any validators set for this registration, let's check those now. */
	if (!M_str_isempty(value) && !reg_validate_value(conf, reg, value, err_buf, sizeof(err_buf))) {
		conf_log_error(conf, "Key '%s' failed validation for value '%s': %s", reg->key, value, err_buf);
		return M_FALSE;
	}
	conf_log_debug(conf, "  Value passed validation");

	/* If we're here, then we can go ahead and set the value. If there is no value for this key, then we'll use the
 	 * default value (if set). */
	reg_set_value(conf, reg, value);

	return M_TRUE;
}


/* --- Conf objects --- */

M_conf_t *M_conf_create(const char *path, M_bool allow_multiple)
{
	M_conf_t         *conf;
	M_ini_settings_t *ini_settings;
	M_list_str_t     *keys;
	size_t            num_keys;
	size_t            i;
	const char       *key;
	M_uint64          num;

	if (M_str_isempty(path))
		return NULL;

	conf = M_malloc_zero(sizeof(*conf));

	conf->allow_multiple = allow_multiple;
	conf->ini_path       = M_strdup(path);

	/* Read in the config file. */
	ini_settings = conf_build_ini_settings(allow_multiple);
	conf->ini    = M_ini_read_file(path, ini_settings, M_TRUE, NULL, 4*1024*1024 /* 4 MB */ );
	M_ini_settings_destroy(ini_settings);

	if (conf->ini == NULL) {
		M_conf_destroy(conf);
		return NULL;
	}

	/* Create the list that we'll use for holding on to registrations until we're ready to run through them. */
	conf->registrations = M_list_create(NULL, M_LIST_NONE);

	/* Create the table that we'll use for keeping track of how many times a key is used. Every time a key is used,
 	 * we'll decrement the count by one. */
	keys              = M_ini_kv_keys(conf->ini);
	num_keys          = M_list_str_len(keys);
	conf->unused_keys = M_hash_stru64_create(num_keys * 2, 75, M_HASH_DICT_CASECMP);
	for (i=0; i<num_keys; i++) {
		key = M_list_str_at(keys, i);
		num = M_hash_stru64_get_direct(conf->unused_keys, key);
		if (num > 0 && !allow_multiple) {
			conf_log_error(conf, "%s is registered multiple times in %s", key, path);
		} else {
			M_hash_stru64_insert(conf->unused_keys, key, num + 1);
		}
	}
	M_list_str_destroy(keys);

	/* Create the list that we'll use for holding on to validator callbacks until we're ready to run through them. */
	conf->validators = M_list_create(NULL, M_LIST_NONE);

	/* Create the list that we'll iterate through for logging debug messages. */
	conf->debug_loggers = M_list_create(NULL, M_LIST_NONE);

	/* Create the list that we'll iterate through for logging error messages. */
	conf->error_loggers = M_list_create(NULL, M_LIST_NONE);

	return conf;
}

void M_conf_destroy(M_conf_t *conf)
{
	M_list_str_t *keys;
	size_t        num_keys;
	size_t        i;

	if (conf != NULL) {
		/* Report the keys that were never used. */
		keys     = M_conf_unused_keys(conf);
		num_keys = M_list_str_len(keys);
		for (i=0; i<num_keys; i++) {
			conf_log_debug(conf, "Unused key: %s", M_list_str_at(keys, i));
		}
		M_list_str_destroy(keys);

		M_free(conf->ini_path);
		M_ini_destroy(conf->ini);
		M_list_destroy(conf->registrations, M_FALSE);
		M_list_destroy(conf->validators, M_FALSE);
		M_list_destroy(conf->debug_loggers, M_FALSE);
		M_list_destroy(conf->error_loggers, M_FALSE);
		M_hash_stru64_destroy(conf->unused_keys);
		M_free(conf);
	}
}

M_bool M_conf_add_debug_logger(M_conf_t *conf, M_conf_logger_t debug_logger)
{
	if (conf == NULL || conf->debug_loggers == NULL || debug_logger == NULL)
		return M_FALSE;

	return M_list_insert(conf->debug_loggers, debug_logger);
}

M_bool M_conf_add_error_logger(M_conf_t *conf, M_conf_logger_t error_logger)
{
	if (conf == NULL || conf->error_loggers == NULL || error_logger == NULL)
		return M_FALSE;

	return M_list_insert(conf->error_loggers, error_logger);
}

M_bool M_conf_parse(M_conf_t *conf)
{
	size_t              num_items;
	unsigned int        i;
	M_conf_reg_t       *reg;
	M_bool              ret = M_TRUE;
	M_conf_validator_t  validator;

	conf_log_debug(conf, "Beginning parse");

	/* Go through all the registrations and set the values. We're going to hit every registration without stopping at
 	 * errors so we can log all errors. */
	while (M_list_len(conf->registrations) > 0) {
		reg = M_list_take_first(conf->registrations);
		if (!reg_handle(conf, reg)) {
			ret = M_FALSE;
		}
		reg_destroy(reg);
	}

	/* Now that all the values are set, let's run through the registered validators and make sure that everything looks
 	 * good. We're going to hit every callback without stopping at errors so we can log all errors. */
	num_items = M_list_len(conf->validators);
	if (num_items > 0)
		conf_log_debug(conf, "Values parsed, running custom validators");
	for (i=0; i<num_items; i++) {
		validator = M_list_at(conf->validators, i);
		if (validator != NULL && !validator()) {
			ret = M_FALSE;
		}
	}

	conf_log_debug(conf, "Finished parse");

	return ret;
}

M_list_str_t *M_conf_unused_keys(M_conf_t *conf)
{
	M_hash_stru64_enum_t *hashenum;
	M_list_str_t         *keys = NULL;
	const char           *key;
	M_uint64              num;

	if (conf != NULL) {
		if (M_hash_stru64_enumerate(conf->unused_keys, &hashenum) > 0) {
			keys = M_list_str_create(M_LIST_STR_CASECMP);
			while (M_hash_stru64_enumerate_next(conf->unused_keys, hashenum, &key, &num)) {
				/* If a key is allowed to have multiple values, then num will be the number of values it has. Otherwise,
 				 * num will be 1. */
				for ( ; num > 0; num--) {
					M_list_str_insert(keys, key);
				}
			}
			M_hash_stru64_enumerate_free(hashenum);
		}
	}

	return keys;
}

M_list_str_t *M_conf_get_sections(M_conf_t *conf)
{
	if (conf == NULL || conf->ini == NULL)
		return NULL;

	return M_ini_kv_sections(conf->ini);
}

const char *M_conf_get_value(M_conf_t *conf, const char *key)
{
	if (conf == NULL || conf->ini == NULL || M_str_isempty(key))
		return NULL;

	/* Mark this key as being used once. */
	conf_decrement_key(conf, key, M_FALSE);

	return M_ini_kv_get_direct(conf->ini, key, 0);
}

M_list_str_t *M_conf_get_values(M_conf_t *conf, const char *key)
{
	if (conf == NULL || conf->ini == NULL || M_str_isempty(key))
		return NULL;

	/* Mark all instances of this key as being used. */
	conf_decrement_key(conf, key, M_TRUE);

	return M_ini_kv_get_vals(conf->ini, key);
}

M_bool M_conf_register_buf(M_conf_t *conf, const char *key, char *buf, size_t buf_len, const char *default_val, const char *regex, M_conf_converter_buf_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || buf == NULL || buf_len == 0)
		return M_FALSE;

	reg                  = reg_create(key, M_CONF_REG_TYPE_BUF);
	reg->mem.buf         = buf;
	reg->buf_len         = buf_len;
	reg->default_val.buf = M_strdup(default_val);
	reg->regex           = M_strdup(regex);
	reg->converter.buf   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_strdup(M_conf_t *conf, const char *key, char **address, const char *default_val, const char *regex, M_conf_converter_strdup_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || address == NULL)
		return M_FALSE;

	reg                     = reg_create(key, M_CONF_REG_TYPE_STRDUP);
	reg->mem.strdup         = address;
	reg->default_val.strdup = M_strdup(default_val);
	reg->regex              = M_strdup(regex);
	reg->converter.strdup   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_int8(M_conf_t *conf, const char *key, M_int8 *mem, M_int8 default_val, M_int8 min_val, M_int8 max_val, M_conf_converter_int8_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                   = reg_create(key, M_CONF_REG_TYPE_INT8);
	reg->mem.int8         = mem;
	reg->default_val.int8 = default_val;
	reg->min_sval         = min_val;
	reg->max_sval         = max_val;
	reg->converter.int8   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_int16(M_conf_t *conf, const char *key, M_int16 *mem, M_int16 default_val, M_int16 min_val, M_int16 max_val, M_conf_converter_int16_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                    = reg_create(key, M_CONF_REG_TYPE_INT16);
	reg->mem.int16         = mem;
	reg->default_val.int16 = default_val;
	reg->min_sval          = min_val;
	reg->max_sval          = max_val;
	reg->converter.int16   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_int32(M_conf_t *conf, const char *key, M_int32 *mem, M_int32 default_val, M_int32 min_val, M_int32 max_val, M_conf_converter_int32_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                    = reg_create(key, M_CONF_REG_TYPE_INT32);
	reg->mem.int32         = mem;
	reg->default_val.int32 = default_val;
	reg->min_sval          = min_val;
	reg->max_sval          = max_val;
	reg->converter.int32   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_int64(M_conf_t *conf, const char *key, M_int64 *mem, M_int64 default_val, M_int64 min_val, M_int64 max_val, M_conf_converter_int64_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                    = reg_create(key, M_CONF_REG_TYPE_INT64);
	reg->mem.int64         = mem;
	reg->default_val.int64 = default_val;
	reg->min_sval          = min_val;
	reg->max_sval          = max_val;
	reg->converter.int64   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_uint8(M_conf_t *conf, const char *key, M_uint8 *mem, M_uint8 default_val, M_uint8 min_val, M_uint8 max_val, M_conf_converter_uint8_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                    = reg_create(key, M_CONF_REG_TYPE_UINT8);
	reg->mem.uint8         = mem;
	reg->default_val.uint8 = default_val;
	reg->min_uval          = min_val;
	reg->max_uval          = max_val;
	reg->converter.uint8   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_uint16(M_conf_t *conf, const char *key, M_uint16 *mem, M_uint16 default_val, M_uint16 min_val, M_uint16 max_val, M_conf_converter_uint16_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                     = reg_create(key, M_CONF_REG_TYPE_UINT16);
	reg->mem.uint16         = mem;
	reg->default_val.uint16 = default_val;
	reg->min_uval           = min_val;
	reg->max_uval           = max_val;
	reg->converter.uint16   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_uint32(M_conf_t *conf, const char *key, M_uint32 *mem, M_uint32 default_val, M_uint32 min_val, M_uint32 max_val, M_conf_converter_uint32_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                     = reg_create(key, M_CONF_REG_TYPE_UINT32);
	reg->mem.uint32         = mem;
	reg->default_val.uint32 = default_val;
	reg->min_uval           = min_val;
	reg->max_uval           = max_val;
	reg->converter.uint32   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_uint64(M_conf_t *conf, const char *key, M_uint64 *mem, M_uint64 default_val, M_uint64 min_val, M_uint64 max_val, M_conf_converter_uint64_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                     = reg_create(key, M_CONF_REG_TYPE_UINT64);
	reg->mem.uint64         = mem;
	reg->default_val.uint64 = default_val;
	reg->min_uval           = min_val;
	reg->max_uval           = max_val;
	reg->converter.uint64   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_bool(M_conf_t *conf, const char *key, M_bool *mem, M_bool default_val, M_conf_converter_bool_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || mem == NULL)
		return M_FALSE;

	reg                      = reg_create(key, M_CONF_REG_TYPE_BOOL);
	reg->mem.boolean         = mem;
	reg->default_val.boolean = default_val;
	reg->converter.boolean   = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_custom(M_conf_t *conf, const char *key, M_conf_converter_custom_t converter)
{
	M_conf_reg_t *reg;

	if (conf == NULL || conf->registrations == NULL || M_str_isempty(key) || converter == NULL)
		return M_FALSE;

	reg                   = reg_create(key, M_CONF_REG_TYPE_CUSTOM);
	reg->converter.custom = converter;

	return M_list_insert(conf->registrations, reg);
}

M_bool M_conf_register_validator(M_conf_t *conf, M_conf_validator_t validator)
{
	if (conf == NULL || conf->validators == NULL || validator == NULL)
		return M_FALSE;

	return M_list_insert(conf->validators, validator);
}
