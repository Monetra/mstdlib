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
#include "data/m_getopt_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_getopt_parse_option_value_integer(M_getopt_option_t *opt, const char *val, void *thunk)
{
	M_int64            val_int = 0;
	M_str_int_retval_t ret_int;

	if (val != NULL) {
		ret_int = M_str_to_int64_ex(val, M_str_len(val), 10, &val_int, NULL);
		if (ret_int != M_STR_INT_SUCCESS) {
			return M_FALSE;
		}
	}
	return opt->cb.integer_cb(opt->short_opt, opt->long_opt, val==NULL?NULL:&val_int, thunk);
}

static M_bool M_getopt_parse_option_value_decimal(M_getopt_option_t *opt, const char *val, void *thunk)
{
	M_decimal_t           val_dec;
	enum M_DECIMAL_RETVAL ret_dec;

	M_decimal_create(&val_dec);
	if (val != NULL) {
		ret_dec = M_decimal_from_str(val, M_str_len(val), &val_dec, NULL);
		if (ret_dec != M_DECIMAL_SUCCESS) {
			return M_FALSE;
		}
	}
	return opt->cb.decimal_cb(opt->short_opt, opt->long_opt, val==NULL?NULL:&val_dec, thunk);
}

static M_bool M_getopt_parse_option_value(M_getopt_option_t *opt, const char *val, void *thunk)
{
	switch (opt->type) {
		case M_GETOPT_TYPE_INTEGER:
			return M_getopt_parse_option_value_integer(opt, val, thunk);
		case M_GETOPT_TYPE_DECIMAL:
			return M_getopt_parse_option_value_decimal(opt, val, thunk);
		case M_GETOPT_TYPE_STRING:
			return opt->cb.string_cb(opt->short_opt, opt->long_opt, val, thunk);
		case M_GETOPT_TYPE_BOOLEAN:
			return opt->cb.boolean_cb(opt->short_opt, opt->long_opt, val==NULL?M_TRUE:M_str_istrue(val), thunk);
		case M_GETOPT_TYPE_UNKNOWN:
			return M_FALSE;
	}
	return M_FALSE;
}

static M_getopt_error_t M_getopt_parse_option_verify_value(M_getopt_option_t *opt, const char **val, M_bool opt_isval, int *idx)
{
	if (opt->val_required && (val == NULL || *val == NULL))
		return M_GETOPT_ERROR_MISSINGVALUE;

	if (val != NULL && opt->type == M_GETOPT_TYPE_BOOLEAN && !opt->val_required) {
		if (opt_isval) {
			return M_GETOPT_ERROR_INVALIDDATATYPE;
		}
		*val = NULL;
	}

	if (val != NULL && *val != NULL && !opt_isval)
		(*idx)++;

	return M_GETOPT_ERROR_SUCCESS;
}

static M_getopt_error_t M_getopt_parse_option_long(M_getopt_t *g, const char *option, const char *val, M_bool opt_isval, int *idx, void *thunk)
{
	M_getopt_option_t *opt;
	M_getopt_error_t   ret;

	opt = M_hash_strvp_get_direct(g->long_opts, option);
	if (opt == NULL)
		return M_GETOPT_ERROR_INVALIDOPT;

	ret = M_getopt_parse_option_verify_value(opt, val==NULL?NULL:&val, opt_isval, idx);
	if (ret != M_GETOPT_ERROR_SUCCESS)
		return ret;

	if (!M_getopt_parse_option_value(opt, val, thunk))
		return M_GETOPT_ERROR_INVALIDDATATYPE;

	return M_GETOPT_ERROR_SUCCESS;
}

static M_getopt_error_t M_getopt_parse_option_short(M_getopt_t *g, const char *option, const char *val, M_bool opt_isval, int *idx, void *thunk)
{
	M_getopt_option_t *opt;
	M_getopt_error_t   ret;
	size_t             len;
	size_t             i;
	char               opt_opt[2];
	const char        *opt_val= NULL;
	M_bool             opt_isval_opt = M_FALSE;

	/* If we have multiple short options together we need to parse out each one. */
	len = M_str_len(option);
	if (len > 1) {
		M_mem_set(opt_opt, 0, sizeof(opt_opt));

		for (i=0; i<len; i++) {
			opt_opt[0] = option[i];
			if (i == len-1) {
				opt_val       = val;
				opt_isval_opt = opt_isval;
			}

			ret = M_getopt_parse_option_short(g, opt_opt, opt_val, opt_isval_opt, idx, thunk);
			if (ret != M_GETOPT_ERROR_SUCCESS) {
				return ret;
			}
		}
	}

	opt = M_hash_u64vp_get_direct(g->short_opts, (M_uint64)option[0]);
	if (opt == NULL)
		return M_GETOPT_ERROR_INVALIDOPT;

	ret = M_getopt_parse_option_verify_value(opt, val==NULL?NULL:&val, opt_isval, idx);
	if (ret != M_GETOPT_ERROR_SUCCESS)
		return ret;

	if (!M_getopt_parse_option_value(opt, val, thunk))
		return M_GETOPT_ERROR_INVALIDDATATYPE;

	return M_GETOPT_ERROR_SUCCESS;
}

static M_getopt_error_t M_getopt_parse_option(M_getopt_t *g, const char *option, const char *val, M_bool opt_isval, int *idx, void *thunk)
{
	/* Carbon apps launched form the GUI have a special arugment added by OS X.
 	 * It takes the form: -psn_0_1950172. The numbers at the end are unique per application
	 * and is the Process Serial Number
	 * (https://developer.apple.com/legacy/library/documentation/Carbon/Reference/Process_Manager/index.html#//apple_ref/doc/c_ref/ProcessSerialNumber);
	 *
	 * Strip it since it's not a valid option.
	 */
#ifdef __APPLE__
	if (M_str_caseeq_max(option, "-psn_0_", 7))
		return M_GETOPT_ERROR_SUCCESS;
#endif
	if (M_str_eq_max(option, "--", 2))
		return M_getopt_parse_option_long(g, option+2, val, opt_isval, idx, thunk);
	return M_getopt_parse_option_short(g, option+1, val, opt_isval, idx, thunk);
}

static M_bool M_getopt_parse_isoption(const char *option)
{
	if (option != NULL && *option == '-')
		return M_TRUE;
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_getopt_error_t M_getopt_parse(M_getopt_t *g, const char * const *argv, int argc, const char **opt_fail, void *thunk)
{
	M_getopt_error_t  ret;
	M_bool            isopt;
	M_bool            opts_done    = M_FALSE;
	M_bool            process_opts = M_TRUE;
	M_bool            opt_isval;
	int               i;
	int               opt_idx;
	char             *opt_tmp;
	char             *opt_opt;
	char             *opt_val;
	char            **parts;
	size_t            num_parts;
	const char       *f;

	if (g == NULL || argv == NULL || argc == 0)
		return M_GETOPT_ERROR_NONOPTION;

	if (opt_fail == NULL)
		opt_fail = &f;

	/* i=1 because we want to skip argv[0] which is the application name. */
	for (i=1; i<argc; i++) {
		/* Determine if this is an option. */
		isopt = M_FALSE;
		if (M_getopt_parse_isoption(argv[i]))
			isopt = M_TRUE;

		/* Determine if we are treating everything as a nonoption. */
		if (!process_opts || M_str_eq(argv[i], "--")) {
			isopt        = M_FALSE;
			/* if process_opts is true then we have '--' which we want to skip processing. */
			if (process_opts) {
				process_opts = M_FALSE;
				continue;
			}
		}

		/* We have an option after we've determined we shouldn't be processing options.
 		 * It's an option after we've started processing nonoptions. This isn't supported. */
		if (isopt && opts_done) {
			*opt_fail = argv[i];
			return M_GETOPT_ERROR_INVALIDORDER;
		}

		if (isopt) {
			/* Ensure this is a complete option. */
			if (M_str_eq(argv[i], "-") || M_str_eq(argv[i], "--")) {
				*opt_fail = argv[i];
				return M_GETOPT_ERROR_INVALIDOPT;
			}

			/* Determine is the option after this one is a value or another option.
 			 * If it's a value we'll want to send it along. */
			opt_opt   = NULL;
			opt_val   = NULL;
			opt_isval = M_FALSE;

			/* Determine if we have a value as part of an option separated by an =. */
			if (M_str_chr(argv[i], '=')) {
				opt_tmp = M_strdup(argv[i]);
				parts = M_str_explode_str('=', opt_tmp, &num_parts);

				/* Must have two parts and neither can be empty. Catch '-s=s=s' and '-s='. */
				if (num_parts != 2 || (num_parts >= 1 && *parts[0] == '\0') || (num_parts >= 2 && *parts[1] == '\0')) {
					*opt_fail = argv[i];
					M_str_explode_free(parts, num_parts);
					M_free(opt_tmp);
					return M_GETOPT_ERROR_MISSINGVALUE;
				}

				opt_opt   = M_strdup(parts[0]);
				opt_val   = M_strdup(parts[1]);
				opt_isval = M_TRUE;

				M_str_explode_free(parts, num_parts);
				M_free(opt_tmp);
			} else {
				opt_opt = M_strdup(argv[i]);
				if (i+1 < argc && !M_getopt_parse_isoption(argv[i+1])) {
					/* Assume that an option followed by a value the value is the value for the option. */
					opt_val = M_strdup(argv[i+1]);
				}
			}

			/* Process the option. */
			opt_idx = i;
			ret     = M_getopt_parse_option(g, opt_opt, opt_val, opt_isval, &i, thunk);
			M_free(opt_val);
			M_free(opt_opt);
			if (ret != M_GETOPT_ERROR_SUCCESS) {
				*opt_fail = argv[opt_idx];
				return ret;
			}
			continue;
		}

		/* We've finished processing all options. */
		opts_done = M_TRUE;

		if (g->nonopt_cb == NULL || !g->nonopt_cb((size_t)i, argv[i], thunk)) {
			*opt_fail = argv[i];
			return M_GETOPT_ERROR_NONOPTION;
		}
	}

	return M_GETOPT_ERROR_SUCCESS;
}
