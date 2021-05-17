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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_hash_dict_t *M_hash_dict_create(size_t size, M_uint8 fillpct, M_uint32 flags)
{
	M_hashtable_hash_func        key_hash     = M_hash_func_hash_str;
	M_sort_compar_t              key_equality = M_sort_compar_str;
	M_hashtable_flags_t          hash_flags   = M_HASHTABLE_NONE;
	struct M_hashtable_callbacks callbacks    = {
		M_hash_void_strdup,
		M_hash_void_strdup,
		M_free,
		M_hash_void_strdup,
		M_hash_void_strdup,
		NULL,
		M_free
	};

	/* Key options. */
	if (flags & M_HASH_DICT_CASECMP) {
		key_hash     = M_hash_func_hash_str_casecmp;
		key_equality = M_sort_compar_str_casecmp;
	}
	if (flags & M_HASH_DICT_KEYS_ORDERED) {
		hash_flags |= M_HASHTABLE_KEYS_ORDERED;
		if (flags & M_HASH_DICT_KEYS_SORTASC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
			if (flags & M_HASH_DICT_CASECMP) {
				key_equality = M_sort_compar_str_casecmp;
			} else {
				key_equality = M_sort_compar_str;
			}
		}
		if (flags & M_HASH_DICT_KEYS_SORTDESC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
			if (flags & M_HASH_DICT_CASECMP) {
				key_equality = M_sort_compar_str_casecmp_desc;
			} else {
				key_equality = M_sort_compar_str_desc;
			}
		}
	}
	if (flags & M_HASH_DICT_KEYS_UPPER) {
		callbacks.key_duplicate_insert = (M_hashtable_duplicate_func)M_strdup_upper;
		callbacks.key_duplicate_copy   = (M_hashtable_duplicate_func)M_strdup_upper;
	}
	if (flags & M_HASH_DICT_KEYS_LOWER) {
		callbacks.key_duplicate_insert = (M_hashtable_duplicate_func)M_strdup_lower;
		callbacks.key_duplicate_copy   = (M_hashtable_duplicate_func)M_strdup_lower;
	}

	/* Multi-value options. */
	if (flags & M_HASH_DICT_MULTI_VALUE) {
		hash_flags |= M_HASHTABLE_MULTI_VALUE;
	}
	if (flags & M_HASH_DICT_MULTI_SORTASC) {
		hash_flags |= M_HASHTABLE_MULTI_SORTED;
		if (flags & M_HASH_DICT_MULTI_CASECMP) {
			callbacks.value_equality = M_sort_compar_str_casecmp;
		} else {
			callbacks.value_equality = M_sort_compar_str;
		}
	}
	if (flags & M_HASH_DICT_MULTI_SORTDESC) {
		hash_flags |= M_HASHTABLE_MULTI_SORTED;
		if (flags & M_HASH_DICT_MULTI_CASECMP) {
			callbacks.value_equality = M_sort_compar_str_casecmp_desc;
		} else {
			callbacks.value_equality = M_sort_compar_str_desc;
		}
	}
	if (flags & M_HASH_DICT_MULTI_GETLAST) {
		hash_flags |= M_HASHTABLE_MULTI_GETLAST;
	}

	/* Initialization options. */
	if (flags & M_HASH_DICT_STATIC_SEED) {
		hash_flags |= M_HASHTABLE_STATIC_SEED;
	}

	/* We are only dealing in opaque types here, and we don't have any
	 * metadata of our own to store, so we are only casting one pointer
	 * type to another.  This is a safe operation */
	return (M_hash_dict_t *)M_hashtable_create(size, fillpct, key_hash, key_equality, hash_flags, &callbacks);
}


void M_hash_dict_destroy(M_hash_dict_t *h)
{
	M_hashtable_destroy((M_hashtable_t *)h, M_TRUE);
}


M_bool M_hash_dict_insert(M_hash_dict_t *h, const char *key, const char *value)
{
	/* Can't insert empty keys. Hashtable base will check for NULL but it won't check for \0 because it
 	 * doesn't know the type. */
	if (key == NULL || *key == '\0')
		return M_FALSE;
	return M_hashtable_insert((M_hashtable_t *)h, key, value);
}


M_bool M_hash_dict_remove(M_hash_dict_t *h, const char *key)
{
	return M_hashtable_remove((M_hashtable_t *)h, key, M_TRUE);
}


M_bool M_hash_dict_get(const M_hash_dict_t *h, const char *key, const char **value)
{
	void   *outval = NULL;
	M_bool  retval;

	retval = M_hashtable_get((const M_hashtable_t *)h, key, &outval);

	if (value != NULL)
		*value = outval;

	return retval;
}


const char *M_hash_dict_get_direct(const M_hash_dict_t *h, const char *key)
{
	const char *val = NULL;

	if (!M_hash_dict_get(h, key, &val))
		return NULL;

	return val;
}


const char *M_hash_dict_get_direct_default(const M_hash_dict_t *h, const char *key, const char *def)
{
	const char *val;
	val = M_hash_dict_get_direct(h, key);
	if (val == NULL)
		return def;
	return val;
}


M_bool M_hash_dict_is_multi(const M_hash_dict_t *h)
{
	return M_hashtable_is_multi((const M_hashtable_t *)h);
}


M_bool M_hash_dict_multi_len(const M_hash_dict_t *h, const char *key, size_t *len)
{
	return M_hashtable_multi_len((const M_hashtable_t *)h, key, len);
}


M_bool M_hash_dict_multi_get(const M_hash_dict_t *h, const char *key, size_t idx, const char **value)
{
	void   *outval = NULL;
	M_bool  retval;

	retval = M_hashtable_multi_get((const M_hashtable_t *)h, key, idx, &outval);

	if (value != NULL)
		*value = outval;

	return retval;
}


const char *M_hash_dict_multi_get_direct(const M_hash_dict_t *h, const char *key, size_t idx)
{
	const char *val = NULL;
	M_hash_dict_multi_get(h, key, idx, &val);
	return val;
}


M_bool M_hash_dict_multi_remove(M_hash_dict_t *h, const char *key, size_t idx)
{
	return M_hashtable_multi_remove((M_hashtable_t *)h, key, idx, M_TRUE);
}


M_uint32 M_hash_dict_size(const M_hash_dict_t *h)
{
	return M_hashtable_size((const M_hashtable_t *)h);
}


size_t M_hash_dict_num_collisions(const M_hash_dict_t *h)
{
	return M_hashtable_num_collisions((const M_hashtable_t *)h);
}


size_t M_hash_dict_num_expansions(const M_hash_dict_t *h)
{
	return M_hashtable_num_expansions((const M_hashtable_t *)h);
}


size_t M_hash_dict_num_keys(const M_hash_dict_t *h)
{
	return M_hashtable_num_keys((const M_hashtable_t *)h);
}


size_t M_hash_dict_enumerate(const M_hash_dict_t *h, M_hash_dict_enum_t **hashenum)
{
	size_t              rv;
	M_hashtable_enum_t *myhashenum = M_malloc(sizeof(*myhashenum));

	*hashenum = (M_hash_dict_enum_t *)myhashenum;
	rv        = M_hashtable_enumerate((const M_hashtable_t *)h, myhashenum);

	/* Caller may not expect hashenum to be initialized if 0 results */
	if (rv == 0) {
		M_free(myhashenum);
		*hashenum = NULL;
	}

	return rv;
}


M_bool M_hash_dict_enumerate_next(const M_hash_dict_t *h, M_hash_dict_enum_t *hashenum, const char **key, const char **value)
{
	return M_hashtable_enumerate_next((const M_hashtable_t *)h, (M_hashtable_enum_t *)hashenum, (const void **)key, (const void **)value);
}


void M_hash_dict_enumerate_free(M_hash_dict_enum_t *hashenum)
{
	M_free(hashenum);
}


void M_hash_dict_merge(M_hash_dict_t **dest, M_hash_dict_t *src)
{
	M_hashtable_merge((M_hashtable_t **)dest, (M_hashtable_t *)src);
}


M_hash_dict_t *M_hash_dict_duplicate(const M_hash_dict_t *h)
{
	return (M_hash_dict_t *)M_hashtable_duplicate((const M_hashtable_t *)h);
}


typedef enum {
	M_HASHDICT_QUOTE_TYPE_OFF = 1,
	M_HASHDICT_QUOTE_TYPE_ON  = 2,
	M_HASHDICT_QUOTE_TYPE_ESCAPE = 3
} M_hashdict_quote_type_t;


static M_hashdict_quote_type_t M_hash_dict_serialize_quotetype(const char *val, size_t val_len, char delim, char kv_delim, char quote, char escape, M_uint32 flags)
{
	size_t                  i;
	M_hashdict_quote_type_t quote_type = (flags & M_HASH_DICT_SER_FLAG_ALWAYS_QUOTE)?M_HASHDICT_QUOTE_TYPE_ON:M_HASHDICT_QUOTE_TYPE_OFF;

	/* Empty, non-null strings get quoted to indicate they're zero-length strings, not NULL. */
	if (val != NULL && M_str_isempty(val))
		quote_type = M_HASHDICT_QUOTE_TYPE_ON;

	if (val == NULL)
		return quote_type;

	/* Beginning or ending with space needs to be quoted so it isn't stripped */
	if (M_chr_isspace(val[0]) || M_chr_isspace(val[val_len-1])) {
		quote_type = M_HASHDICT_QUOTE_TYPE_ON;
	}

	for (i=0; i<val_len; i++) {
		if (quote_type == M_HASHDICT_QUOTE_TYPE_OFF) {
			if ((flags & M_HASH_DICT_SER_FLAG_QUOTE_NON_ANS && !M_chr_isalnumsp(val[i])) ||
			    (val[i] == delim || val[i] == kv_delim)) {
				quote_type = M_HASHDICT_QUOTE_TYPE_ON;
			}
		}

		if (val[i] == quote || val[i] == escape || (!M_chr_isprint(val[i]) && flags & M_HASH_DICT_SER_FLAG_HEXENCODE_NONPRINT)) {
			return M_HASHDICT_QUOTE_TYPE_ESCAPE;
		}
	}
	return quote_type;
}


M_bool M_hash_dict_serialize_buf(M_hash_dict_t *dict, M_buf_t *buf, char delim, char kv_delim, char quote, char escape, M_uint32 flags)
{
	M_hash_dict_enum_t *hashenum = NULL;
	const char         *key;
	const char         *val;

	/* Error */
	if (dict == NULL)
		return M_FALSE;

	/* Blank is ok, should output empty string */
	if (M_hash_dict_num_keys(dict) == 0)
		return M_TRUE;

	M_hash_dict_enumerate(dict, &hashenum);
	while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
		M_hashdict_quote_type_t quote_type = M_hash_dict_serialize_quotetype(val, M_str_len(val), delim, kv_delim, quote, escape, flags);

		/* Output Key */
		M_buf_add_str(buf, key);

		/* Output delim between key and value */
		if (flags & M_HASH_DICT_SER_FLAG_LF_TO_CRLF && kv_delim == '\n') {
			M_buf_add_bytes(buf, "\r\n", 2);
		} else {
			M_buf_add_byte(buf, (unsigned char)kv_delim);
		}

		/* Output quote if necessary */
		if (quote_type != M_HASHDICT_QUOTE_TYPE_OFF)
			M_buf_add_byte(buf, (unsigned char)quote);

		/* Output value, if we know it needs to be escaped, loop over it */
		if (quote_type == M_HASHDICT_QUOTE_TYPE_ESCAPE) {
			for ( ; *val != '\0'; val++) {
				if (*val == quote || *val == escape)
					M_buf_add_byte(buf, (unsigned char)escape);

				if (!M_chr_isprint(*val) && flags & M_HASH_DICT_SER_FLAG_HEXENCODE_NONPRINT) {
					M_bprintf(buf, "[%02X]", (unsigned int)*val);
				} else {
					M_buf_add_byte(buf, (unsigned char)*val);
				}
			}
		} else {
			M_buf_add_str(buf, val);
		}

		/* Output quote if necessary  */
		if (quote_type != M_HASHDICT_QUOTE_TYPE_OFF)
			M_buf_add_byte(buf, (unsigned char)quote);

		/* Output delimiter at end of each value */
		if (flags & M_HASH_DICT_SER_FLAG_LF_TO_CRLF && delim == '\n') {
			M_buf_add_bytes(buf, "\r\n", 2);
		} else {
			M_buf_add_byte(buf, (unsigned char)delim);
		}
	}

	M_hash_dict_enumerate_free(hashenum);
	return M_TRUE;
}


char *M_hash_dict_serialize(M_hash_dict_t *dict, char delim, char kv_delim, char quote, char escape, M_uint32 flags)
{
	M_buf_t *buf = M_buf_create();
	char    *out = NULL;

	if (!M_hash_dict_serialize_buf(dict, buf, delim, kv_delim, quote, escape, flags)) {
		M_buf_cancel(buf);
		return NULL;
	}

	out = M_buf_finish_str(buf, NULL);

	/* We want to make sure it outputs an empty string not NULL on no entries */
	if (out == NULL)
		out = M_strdup("");

	return out;
}


static char *M_hash_dict_fromstr_unquote(const char *str, char quote, char escape)
{
	size_t len;
	size_t i;
	char  *ret;
	size_t cnt       = 0;
	M_bool on_escape = M_FALSE;

	if (M_str_isempty(str))
		return NULL;

	len = M_str_len(str);

	/* remove surrounding quotes */
	if (len >=2 && str[0] == quote && str[len-1] == quote) {
		str++;
		len-=2;
	}

	ret = M_malloc_zero(len+1);

	/* remove duplicate quotes */
	for (i=0; i<len; i++) {
		/* quotes escape other quotes */
		if (!on_escape && str[i] == escape) {
			on_escape = M_TRUE;
		} else {
			ret[cnt++] = str[i];
			on_escape  = M_FALSE;
		}
	}

	return ret;
}


M_hash_dict_t *M_hash_dict_deserialize(const char *str, size_t len, char delim, char kv_delim, char quote, char escape, M_uint32 flags)
{
	M_hash_dict_t  *dict    = NULL;
	char          **kvs     = NULL;
	size_t          num_kvs = 0;
	char          **kv      = NULL;
	size_t          num_kv  = 0;
	size_t          i;
	M_bool          success = M_FALSE;

	if (M_str_isempty(str)) {
		return NULL;
	}
	kvs = M_str_explode_quoted((unsigned char)delim, str, len, (unsigned char)quote, (unsigned char)escape, 0, &num_kvs, NULL);
	if (kvs == NULL) {
		goto cleanup;
	}

	dict = M_hash_dict_create(16, 75, flags);
	for (i=0; i<num_kvs; i++) {
		char *temp;

		if (flags & M_HASH_DICT_DESER_TRIM_WHITESPACE) {
			/* Discard leading and trailing whitespace.  Trailing whitespace might be something like '\r' */
			M_str_trim(kvs[i]);
		}

		/* Skip blank lines, should really only be the last line */
		if (M_str_isempty(kvs[i]))
			continue;

		kv = M_str_explode_str_quoted((unsigned char)kv_delim, kvs[i], (unsigned char)quote, (unsigned char)escape, 2, &num_kv);
		if (kv == NULL || num_kv != 2) {
			goto cleanup;
		}

		if (flags & M_HASH_DICT_DESER_TRIM_WHITESPACE) {
			/* Trim whitespace from both key and value. Quotes should have been used to protect any legit leading or trailing spaces. */
			M_str_trim(kv[0]);
			M_str_trim(kv[1]);
		}

		/* Remove quotes */
		temp = M_hash_dict_fromstr_unquote(kv[1], quote, escape);
		M_hash_dict_insert(dict, kv[0], temp);
		M_free(temp);

		M_str_explode_free(kv, num_kv);
		kv = NULL; num_kv = 0;
	}

	success = M_TRUE;

cleanup:
	M_str_explode_free(kvs, num_kvs);
	M_str_explode_free(kv, num_kv);
	if (!success) {
		M_hash_dict_destroy(dict);
		dict = NULL;
	}

	return dict;
}
