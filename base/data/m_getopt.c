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
#include "data/m_getopt_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_getopt_validate_short_opt(char short_opt)
{
	if (short_opt == 0)
		return M_TRUE;

	if (!M_chr_isgraph(short_opt))
		return M_FALSE;

	if (short_opt == '-' || short_opt == '=' || short_opt == '"' || short_opt == '\'')
		return M_FALSE;

	return M_TRUE;
}

static M_bool M_getopt_validate_long_opt(const char *long_opt)
{
	if (long_opt == NULL)
		return M_TRUE;

	if (*long_opt == '\0')
		return M_FALSE;

	if (!M_str_isgraph(long_opt))
		return M_FALSE;

	if (*long_opt == '-' || long_opt[M_str_len(long_opt)-1] == '-')
		return M_FALSE;

	if (M_str_chr(long_opt, '=') != NULL || M_str_chr(long_opt, '"') != NULL || M_str_chr(long_opt, '\'') != NULL)
		return M_FALSE;

	return M_TRUE;
}

static M_getopt_option_t *M_getopt_create_option(M_getopt_type_t type, char short_opt, const char *long_opt, M_bool val_required, const char *description)
{
	M_getopt_option_t *opt;

	/* Validate we have a short, long or both and they are valid. */
	if ((short_opt == 0 && long_opt == NULL) ||
		!M_getopt_validate_short_opt(short_opt) || !M_getopt_validate_long_opt(long_opt))
	{
		return NULL;
	}

	opt               = M_malloc_zero(sizeof(*opt));
	opt->type         = type;
	opt->short_opt    = short_opt;
	opt->long_opt     = long_opt!=NULL?M_strdup(long_opt):NULL;
	opt->val_required = val_required;
	opt->description  = M_strdup(description);

	return opt;
}

static void M_getopt_destroy_option(M_getopt_option_t *opt)
{
	if (opt == NULL)
		return;

	M_free(opt->long_opt);
	M_free(opt->description);

	M_free(opt);
}

static M_bool M_getopt_append_option(M_getopt_t *g, M_getopt_option_t *opt)
{
	if (g == NULL || opt == NULL)
		return M_FALSE;

	/* Check if this option has already been added. */
	if ((opt->short_opt != 0 && M_hash_u64vp_get(g->short_opts, (M_uint64)opt->short_opt, NULL)) || (g->long_opts != NULL && M_hash_strvp_get(g->long_opts, opt->long_opt, NULL)))
		return M_FALSE;

	if (opt->short_opt != 0)
		M_hash_u64vp_insert(g->short_opts, (M_uint64)opt->short_opt, opt);
	if (opt->long_opt != NULL)
		M_hash_strvp_insert(g->long_opts, opt->long_opt, opt);

	if (g->last == NULL) {
		g->first      = opt;
		g->last       = opt;
	} else {
		g->last->next = opt;
		g->last       = opt;
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_getopt_t *M_getopt_create(M_getopt_nonopt_cb cb)
{
	M_getopt_t *g;

	g = M_malloc_zero(sizeof(*g));
	g->short_opts  = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, NULL);
	g->long_opts   = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP, NULL);
	g->nonopt_cb   = cb;

	return g;
}

void M_getopt_destroy(M_getopt_t *g)
{
	M_getopt_option_t *opt;
	M_getopt_option_t *next;

	if (g == NULL)
		return;

	M_hash_u64vp_destroy(g->short_opts, M_FALSE);
	M_hash_strvp_destroy(g->long_opts, M_FALSE);

	opt = g->first;
	while (opt != NULL) {
		/* Store the next option because the option will be destroyed. */
		next = opt->next;
		M_getopt_destroy_option(opt);
		opt = next;
	}

	M_free(g);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_getopt_help(const M_getopt_t *g)
{
	M_buf_t           *buf;
	M_getopt_option_t *opt;
	M_bool             has_short;

	if (g == NULL || g->first == NULL)
		return NULL;

	buf = M_buf_create();

	opt = g->first;
	while (opt != NULL) {
		has_short = M_FALSE;

		M_buf_add_fill(buf, ' ', 2);

		if (opt->short_opt != 0) {
			has_short = M_TRUE;
			M_buf_add_byte(buf, '-');
			M_buf_add_char(buf, opt->short_opt);
		}
		if (opt->long_opt != NULL) {
			if (has_short) {
				M_buf_add_str(buf, ", ");
			}
			M_buf_add_str(buf, "--");
			M_buf_add_str(buf, opt->long_opt);
		}

		if (opt->type != M_GETOPT_TYPE_BOOLEAN || (opt->type == M_GETOPT_TYPE_BOOLEAN && opt->val_required)) {
			M_buf_add_byte(buf, ' ');
			if (opt->val_required) {
				M_buf_add_str(buf, "<val>");
			} else {
				M_buf_add_str(buf, "[val]");
			}

			M_buf_add_byte(buf, ' ');

			M_buf_add_byte(buf, '(');
			switch (opt->type) {
				case M_GETOPT_TYPE_INTEGER:
					M_buf_add_str(buf, "integer");
					break;
				case M_GETOPT_TYPE_DECIMAL:
					M_buf_add_str(buf, "decimal");
					break;
				case M_GETOPT_TYPE_STRING:
					M_buf_add_str(buf, "string");
					break;
				case M_GETOPT_TYPE_BOOLEAN:
					M_buf_add_str(buf, "boolean");
					break;
				case M_GETOPT_TYPE_UNKNOWN:
					break;
			}
			M_buf_add_byte(buf, ')');
		}

		if (opt->description != NULL) {
			M_buf_add_byte(buf, ' ');
			M_buf_add_str(buf, opt->description);
		}

		M_buf_add_byte(buf, '\n');
		opt = opt->next;
	}

	return M_buf_finish_str(buf, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_getopt_addinteger(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_integer_cb cb)
{
	M_getopt_option_t *opt;

	if (g == NULL || cb == NULL)
		return M_FALSE;

	opt = M_getopt_create_option(M_GETOPT_TYPE_INTEGER, short_opt, long_opt, val_required, description);
	if (opt == NULL)
		return M_FALSE;
	opt->cb.integer_cb = cb;

	if (!M_getopt_append_option(g, opt)) {
		M_getopt_destroy_option(opt);
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_getopt_adddecimal(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_decimal_cb cb)
{
	M_getopt_option_t *opt;

	if (g == NULL || cb == NULL)
		return M_FALSE;

	opt = M_getopt_create_option(M_GETOPT_TYPE_DECIMAL, short_opt, long_opt, val_required, description);
	if (opt == NULL)
		return M_FALSE;
	opt->cb.decimal_cb = cb;

	if (!M_getopt_append_option(g, opt)) {
		M_getopt_destroy_option(opt);
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_getopt_addstring(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_string_cb cb)
{
	M_getopt_option_t *opt;

	if (g == NULL || cb == NULL)
		return M_FALSE;

	opt = M_getopt_create_option(M_GETOPT_TYPE_STRING, short_opt, long_opt, val_required, description);
	if (opt == NULL)
		return M_FALSE;
	opt->cb.string_cb = cb;

	if (!M_getopt_append_option(g, opt)) {
		M_getopt_destroy_option(opt);
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_getopt_addboolean(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_boolean_cb cb)
{
	M_getopt_option_t *opt;

	if (g == NULL || cb == NULL)
		return M_FALSE;

	opt = M_getopt_create_option(M_GETOPT_TYPE_BOOLEAN, short_opt, long_opt, val_required, description);
	if (opt == NULL)
		return M_FALSE;
	opt->cb.boolean_cb = cb;

	if (!M_getopt_append_option(g, opt)) {
		M_getopt_destroy_option(opt);
		return M_FALSE;
	}
	return M_TRUE;
}
