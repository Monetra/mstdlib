/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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


#include "m_http_header.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	char          *value;
	M_hash_dict_t *modifiers; /* modifier key and value. value will be "" if a flag. */
} M_http_header_value_t;

struct M_http_header {
	char           *key;
	M_hash_strvp_t *values; /* value and modifiers */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_http_header_nosplit(const char *key)
{
	if (M_str_isempty(key))
		return M_FALSE;

	if (M_str_caseeq(key, "WWW-Authenticate") || M_str_caseeq(key, "Proxy-Authorization") ||
		M_str_caseeq(key, "Content-Type") || M_str_caseeq(key, "Date"))
	{
		return M_TRUE;
	}
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_header_value_t *M_http_header_value_create(char *value, M_hash_dict_t *modifiers)
{
	M_http_header_value_t  *v;

	if (M_str_isempty(value))
		return NULL;

	v            = M_malloc_zero(sizeof(*v));
	v->value     = value;
	v->modifiers = modifiers;
	return v;
}

static void M_http_header_value_destroy(M_http_header_value_t *v)
{
	if (v == NULL)
		return;

	M_free(v->value);
	M_hash_dict_destroy(v->modifiers);

	M_free(v);
}

static char *M_http_header_value_string(const M_http_header_value_t *v)
{
	M_list_str_t       *l;
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;
	char               *out;
	M_buf_t            *buf;

	if (v == NULL)
		return NULL;

	l = M_list_str_create(M_LIST_STR_NONE);

	M_list_str_insert(l, v->value);

	buf = M_buf_create();
	M_hash_dict_enumerate(v->modifiers, &he);
	while (M_hash_dict_enumerate_next(v->modifiers, he, &key, &val)) {
		M_buf_add_str(buf, key);

		if (!M_str_isempty(val)) {
			M_buf_add_byte(buf, '=');
			M_buf_add_str(buf, val);
		}

		M_list_str_insert(l, M_buf_peek(buf));
		M_buf_truncate(buf, 0);
	}
	M_hash_dict_enumerate_free(he);
	M_buf_cancel(buf);

	out = M_list_str_join_str(l, "; ");
	M_list_str_destroy(l);
	return out;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_http_header_split_value_and_modifiers(const char *full_value, char **value, M_hash_dict_t **modifiers)
{
	char   **parts;
	size_t   num_parts = 0;
	size_t   i;

	*value     = NULL;
	*modifiers = NULL;

	if (M_str_isempty(full_value))
		return M_FALSE;

	/* Split on ; to look for modifiers. */
	parts = M_str_explode_str(';', full_value, &num_parts);
	if (parts == NULL || num_parts == 0)
		return M_FALSE;

	/* Value is always the first element. */
	*value = M_strdup_trim(parts[0]);

	/* Setup the modifier list. */
	*modifiers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);

	/* Go though the modifiers and add them if there are any. */
	for (i=1; i<num_parts; i++) {
		char   **mparts;
		size_t   num_mparts = 0;
		char    *key;
		char    *val;

		mparts = M_str_explode_str('=', parts[i], &num_mparts);
		if (mparts == NULL || num_mparts == 0) {
			M_str_explode_free(mparts, num_mparts);
			continue;
		}

		key = M_strdup_trim(mparts[0]);
		if (num_mparts >= 2) {
			val = M_strdup_trim(mparts[1]);
		} else {
			val = M_strdup("");
		}

		M_hash_dict_insert(*modifiers, key, val);

		M_free(val);
		M_free(key);
		M_str_explode_free(mparts, num_mparts);
	}

	M_str_explode_free(parts, num_parts);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_header_t *M_http_header_create(const char *key, const char *full_value)
{
	M_http_header_t *h;

	if (M_str_isempty(key) || M_str_isempty(full_value))
		return NULL;

	h         = M_malloc_zero(sizeof(*h));
	h->key    = M_strdup_trim(key);
	h->values = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void (*)(void *))M_http_header_value_destroy);

	if (!M_http_header_update(h, full_value)) {
		M_http_header_destroy(h);
		return NULL;
	}

	return h;
}

void M_http_header_destroy(M_http_header_t *h)
{
	if (h == NULL)
		return;

	M_free(h->key);
	M_hash_strvp_destroy(h->values, M_TRUE);

	M_free(h);
}

M_bool M_http_header_update(M_http_header_t *h, const char *header_value)
{
	M_http_header_value_t  *hval;
	M_list_str_t           *split_header = NULL;
	size_t                  len;
	size_t                  i;

	if (M_str_isempty(header_value))
		return M_FALSE;

	split_header = M_http_split_header_vals(h->key, header_value);
	if (split_header == NULL)
		return M_FALSE;

	len = M_list_str_len(split_header);
	for (i=0; i<len; i++) {
		M_hash_dict_t *modifiers = NULL;
		char          *val       = NULL;

		if (!M_http_header_split_value_and_modifiers(M_list_str_at(split_header, i), &val, &modifiers)) {
			continue;
		}

		hval = M_hash_strvp_get_direct(h->values, val);
		if (hval == NULL) {
			/* Takes ownership of arguments */
			hval = M_http_header_value_create(val, modifiers);
			M_hash_strvp_insert(h->values, val, hval);
			continue;
		}

		M_hash_dict_merge(&hval->modifiers, M_hash_dict_duplicate(modifiers));

		M_hash_dict_destroy(modifiers);
		M_free(val);
	}

	M_list_str_destroy(split_header);
	return M_TRUE;
}

char *M_http_header_value(const M_http_header_t *h)
{
	const M_http_header_value_t *hval;
	M_hash_strvp_enum_t         *he;
	M_list_str_t                *l;
	const char                  *key;
	char                        *out;

	if (h == NULL)
		return NULL;

	l = M_list_str_create(M_LIST_STR_NONE);

	M_hash_strvp_enumerate(h->values, &he);
	while (M_hash_strvp_enumerate_next(h->values, he, &key, (void **)&hval)) {
		out = M_http_header_value_string(hval);
		M_list_str_insert(l, out);
		M_free(out);
	}

	if (M_http_header_nosplit(h->key)) {
		/* We really shouldn't have multiple of these because
		 * they only allow one value. If we do have more than one
		 * there isn't a separator character so we'll just use a space */
		out = M_list_str_join_str(l, " ");
	} else {
		out = M_list_str_join_str(l, ", ");
	}

	M_list_str_destroy(l);
	return out;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_str_t *M_http_split_header_vals(const char *key, const char *header_value)
{
	M_list_str_t  *split_header = NULL;
	char         **parts;
	char          *temp;
	size_t         num_parts    = 0;
	size_t         i;

	if (M_str_isempty(header_value))
		return NULL;

	split_header = M_list_str_create(M_LIST_STR_NONE);

	if (M_http_header_nosplit(key)) {
		temp = M_strdup_trim(header_value);
		M_list_str_insert(split_header, temp);
		M_free(temp);
	} else {
		parts = M_str_explode_str(',', header_value, &num_parts);

		if (parts == NULL || num_parts == 0) {
			M_list_str_destroy(split_header);
			return NULL;
		}

		for (i=0; i<num_parts; i++) {
			temp = M_strdup_trim(parts[i]);
			M_list_str_insert(split_header, temp);
			M_free(temp);
		}

		M_str_explode_free(parts, num_parts);
	}

	return split_header;
}
