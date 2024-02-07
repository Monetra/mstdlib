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

#include "m_re_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    size_t offset;
    size_t len;
} M_re_match_entry_t;

struct M_ret_match {
    /* vp is M_re_match_t */
    M_hash_u64vp_t *idx_captures;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_re_match_entry_t *M_re_match_entry_create(size_t offset, size_t len)
{
    M_re_match_entry_t *entry;

    entry         = M_malloc(sizeof(*entry));
    entry->offset = offset;
    entry->len    = len;

    return entry;
}

static void M_re_match_entry_destroy(M_re_match_entry_t *entry)
{
    if (entry == NULL)
        return;
    M_free(entry);
}

M_re_match_t *M_re_match_create(void)
{
    M_re_match_t *match;

    match               = M_malloc_zero(sizeof(*match));
    match->idx_captures = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void(*)(void *))M_re_match_entry_destroy);

    return match;
}

void M_re_match_destroy(M_re_match_t *match)
{
    if (match == NULL)
        return;

    M_hash_u64vp_destroy(match->idx_captures, M_TRUE);

    M_free(match);
}

void M_re_match_insert(M_re_match_t *match, size_t idx, size_t start, size_t len)
{
    M_re_match_entry_t *entry;

    if (match == NULL)
        return;

    entry = M_re_match_entry_create(start, len);
    M_hash_u64vp_insert(match->idx_captures, idx, entry);
}

M_list_u64_t *M_re_match_idxs(const M_re_match_t *match)
{
    M_hash_u64vp_enum_t *he;
    M_list_u64_t        *l;
    M_uint64             idx;

    if (match == NULL)
        return NULL;

    if (M_hash_u64vp_num_keys(match->idx_captures) == 0)
        return NULL;

    l = M_list_u64_create(M_LIST_U64_SORTASC);

    M_hash_u64vp_enumerate(match->idx_captures, &he);
    while (M_hash_u64vp_enumerate_next(match->idx_captures, he, &idx, NULL)) {
        M_list_u64_insert(l, idx);
    }
    M_hash_u64vp_enumerate_free(he);

    return l;
}

M_bool M_re_match_idx(const M_re_match_t *match, size_t idx, size_t *offset, size_t *len)
{
    const M_re_match_entry_t *entry;
    size_t                    myoffset;
    size_t                    mylen;

    if (match == NULL)
        return M_FALSE;

    if (offset == NULL)
        offset = &myoffset;
    *offset = 0;
    if (len == NULL)
        len = &mylen;
    *len = 0;

    entry = M_hash_u64vp_get_direct(match->idx_captures, idx);
    if (entry == NULL)
        return M_FALSE;

    *offset = entry->offset;
    *len    = entry->len;
    return M_TRUE;
}

void M_re_match_adjust_offset(M_re_match_t *match, size_t adjust)
{
    M_hash_u64vp_enum_t *he;
    M_re_match_entry_t  *entry;

    if (match == NULL)
        return;

    M_hash_u64vp_enumerate(match->idx_captures, &he);
    while (M_hash_u64vp_enumerate_next(match->idx_captures, he, NULL, (void **)&entry)) {
        entry->offset += adjust;
    }
    M_hash_u64vp_enumerate_free(he);
}
