/* The MIT License (MIT)
 *
 * Copyright (c) 2021 Monetra Technologies, LLC.
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
#include "m_defs_int.h"

#include <mstdlib/mstdlib.h>


M_bool M_bitlist_list(char **out, M_bitlist_flags_t flags, const M_bitlist_t *list, M_uint64 bits, unsigned char delim, char *error, size_t error_len)
{
	M_bool   rv = M_FALSE;
	size_t   i;
	M_buf_t *buf;
	if (out == NULL || list == NULL || delim == 0) {
		M_snprintf(error, error_len, "invalid use");
		return M_FALSE;
	}

	buf = M_buf_create();


	for (i=0; list[i].name != NULL; i++) {
		if (!(flags & M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2) && list[i].id != 0 &&
			!M_uint64_is_power_of_two(list[i].id)) {
			M_snprintf(error, error_len, "'%s' is not a power of 2", list[i].name);
			goto done;
		}

		if ((bits & list[i].id) == list[i].id) {
			if (M_buf_len(buf))
				M_buf_add_byte(buf, delim);
			M_buf_add_str(buf, list[i].name);

			/* Remove consumed bits */
			bits &= ~list[i].id;
		}
	}


	if (!(flags & M_BITLIST_FLAG_IGNORE_UNKNOWN) && bits) {
		M_snprintf(error, error_len, "unknown remaining bits 0x%0llX", bits);
		goto done;
	}

	rv = M_TRUE;

done:
	if (rv) {
		*out = M_buf_finish_str(buf, NULL);
		/* If empty, might be NULL, transform to empty string as NULL could mean error to a user */
		if (*out == NULL)
			*out = M_malloc_zero(1);
	} else {
		M_buf_cancel(buf);
	}

	return rv;
}

static M_bool M_bitlist_parse_int(M_uint64 *out, M_bitlist_flags_t flags, const M_bitlist_t *list, M_hash_stru64_t *hash_toint, const char *data, unsigned char delim, char *error, size_t error_len)
{
	char **elems	 = NULL;
	size_t num_elems = 0;
	M_bool rv		= M_FALSE;
	size_t i;

	if (out == NULL || (list == NULL && hash_toint == NULL) || (list != NULL && hash_toint != NULL) || delim == 0) {
		M_snprintf(error, error_len, "invalid use");
		return M_FALSE;
	}

	*out = 0;

	elems = M_str_explode_str(delim, data, &num_elems);
	for (i=0; i<num_elems; i++) {
		M_bool   found = M_FALSE;
		M_uint64 id    = 0;

		/* Trim whitespace */
		if (!(flags & M_BITLIST_FLAG_DONT_TRIM_WHITESPACE)) {
			M_str_trim(elems[i]);
		}

		/* Ignore empty strings */
		if (M_str_isempty(elems[i])) {
			continue;
		}

		if (list) {
			size_t j;

			for (j=0; list[j].name != NULL; j++) {
				if (flags & M_BITLIST_FLAG_CASE_SENSITIVE) {
					found = M_str_eq(list[j].name, elems[i]);
				} else {
					found = M_str_caseeq(list[j].name, elems[i]);
				}
				if (found) {
					id = list[j].id;
					break;
				}
			}
		} else {
			found = M_hash_stru64_get(hash_toint, elems[i], &id);
		}

		if (!found && !(flags & M_BITLIST_FLAG_IGNORE_UNKNOWN)) {
			M_snprintf(error, error_len, "unrecognized value '%s'", elems[i]);
			goto done;
		}
		if (found && !(flags & M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2) && !M_uint64_is_power_of_two(id)) {
			M_snprintf(error, error_len, "'%s' is not a power of 2", elems[i]);
			goto done;
		}

		if (found)
			(*out) |= id;
	}

	rv	= M_TRUE;
done:

	if (!rv)
		*out = 0;

	M_str_explode_free(elems, num_elems);
	return rv;
}


M_bool M_bitlist_tohash(M_hash_stru64_t **hash_toint, M_hash_u64str_t **hash_tostr, M_bitlist_flags_t flags, const M_bitlist_t *list, char *error, size_t error_len)
{
	M_bool           rv    = M_FALSE;
	M_hash_stru64_t *toint = M_hash_stru64_create(16, 75, (flags & M_BITLIST_FLAG_CASE_SENSITIVE)?M_HASH_STRU64_NONE:M_HASH_STRU64_CASECMP);
	M_hash_u64str_t *tostr = M_hash_u64str_create(16, 75, M_HASH_STRU64_NONE);
	size_t           i;

	if (hash_toint == NULL || hash_tostr == NULL || list == NULL) {
		M_snprintf(error, error_len, "invalid use");
		goto done;
	}

	*hash_toint = NULL;
	*hash_tostr = NULL;

	for (i=0; list[i].name != NULL; i++) {
		M_bool is_duplicate = M_FALSE;

		if (M_hash_stru64_get(toint, list[i].name, NULL)) {
			M_snprintf(error, error_len, "duplicate key name %s", list[i].name);
			goto done;
		}

		if (M_hash_u64str_get(tostr, list[i].id, NULL)) {
			if (flags & M_BITLIST_FLAG_IGNORE_DUPLICATE_ID) {
				is_duplicate = M_TRUE;
			} else {
				M_snprintf(error, error_len, "duplicate key id %lld", list[i].id);
				goto done;
			}
		}

		if (list[i].id != 0 && !(flags & M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2) &&
			!M_uint64_is_power_of_two(list[i].id)) {
			M_snprintf(error, error_len, "'%s' is not a power of 2", list[i].name);
			goto done;
		}

		/* All good, insert into both hashtables */
		M_hash_stru64_insert(toint, list[i].name, list[i].id);

		/* Don't insert the same id again, first id wins */
		if (!is_duplicate)
			M_hash_u64str_insert(tostr, list[i].id, list[i].name);
	}

	*hash_toint = toint;
	*hash_tostr = tostr;
	toint       = NULL;
	tostr       = NULL;
	rv          = M_TRUE;

done:
	M_hash_stru64_destroy(toint);
	M_hash_u64str_destroy(tostr);
	return rv;
}


M_bool M_bitlist_parse(M_uint64 *out, M_bitlist_flags_t flags, const M_bitlist_t *list, const char *data, unsigned char delim, char *error, size_t error_len)
{
	return M_bitlist_parse_int(out, flags, list, NULL, data, delim, error, error_len);
}

M_bool M_bitlist_hash_parse(M_uint64 *out, M_bitlist_flags_t flags, M_hash_stru64_t *hash_toint, const char *data, unsigned char delim, char *error, size_t error_len)
{
	return M_bitlist_parse_int(out, flags, NULL, hash_toint, data, delim, error, error_len);
}

const char *M_bitlist_single_tostr(const M_bitlist_t *list, M_uint64 id)
{
	size_t i;

	for (i=0; list[i].name != NULL; i++) {
		if (list[i].id == id)
			return list[i].name;
	}

	return NULL;
}

M_uint64 M_bitlist_single_toint(const M_bitlist_t *list, const char *name)
{
	size_t i;

	for (i=0; list[i].name != NULL; i++) {
		if (M_str_caseeq(list[i].name, name))
			return list[i].id;
	}

	return 0;
}
