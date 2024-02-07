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
#include <mstdlib/mstdlib_formats.h>
#include "ini/m_ini_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The merged ini duplicates the tree structure of the new ini.
 * This is so comments the order for new is used in the merged ini.
 */
static M_ini_t *M_ini_merger_create_merged_ini(const M_ini_t *new_ini)
{
    M_ini_t             *merged_ini;
    M_hash_strvp_enum_t *hashenum;
    const char          *key;
    void                *val;

    merged_ini = M_ini_create(new_ini->ignore_whitespace);

    M_ini_elements_destroy(merged_ini->elements);
    merged_ini->elements = M_ini_elements_duplicate(new_ini->elements);

    /* ini object sections are pass though pointers so we have to manually duplicate the sections. */
    M_hash_strvp_enumerate(new_ini->sections, &hashenum);
    while (M_hash_strvp_enumerate_next(new_ini->sections, hashenum, &key, &val)) {
        M_hash_strvp_insert(merged_ini->sections, key, M_ini_elements_duplicate((M_ini_elements_t *)val));
    }
    M_hash_strvp_enumerate_free(hashenum);

    return merged_ini;
}

static M_list_str_t *M_ini_merger_handle_keys(M_ini_t *merged_ini, const M_ini_t *cur_ini, const M_ini_t *new_ini, const M_ini_t *orig_ini, const M_ini_settings_t *info)
{
    M_list_str_t           *keys;
    M_list_str_t           *update_keys;
    const char             *key;
    size_t                  len;
    size_t                  val_len;
    size_t                  i;
    size_t                  j;
    M_ini_merge_conflict_t  flags;
    M_bool                  in_new;
    M_bool                  in_orig;
    M_ini_merge_resolver_t  resolver;

    update_keys = M_list_str_create(M_LIST_STR_NONE);
    flags       = M_ini_settings_merger_get_conflict_flags(info);
    resolver    = M_ini_settings_merger_get_resolver(info);

    /* 1. Only in new, add. */
    keys = M_ini_kv_keys(new_ini);
    len  = M_list_str_len(keys);
    for (i=0; i<len; i++) {
        key = M_list_str_at(keys, i);
        if (!M_ini_kv_has_key(cur_ini, key) && !M_ini_kv_has_key(orig_ini, key)) {
            val_len = M_ini_kv_len(new_ini, key);
            for (j=0; j<val_len; j++) {
                M_ini_kv_insert(merged_ini, key, M_ini_kv_get_direct(new_ini, key, j));
            }
        }
    }
    M_list_str_destroy(keys);

    keys = M_ini_kv_keys(cur_ini);
    len  = M_list_str_len(keys);
    for (i=0; i<len; i++) {
        key     = M_list_str_at(keys, i);
        in_new  = M_ini_kv_has_key(new_ini, key);
        in_orig = M_ini_kv_has_key(orig_ini, key);

        /* 2. Only in cur, add. */
        /* 3. In cur and new but not in orig, keep. */
        if ((!in_new && !in_orig) ||
            (in_new && !in_orig))
        {
            val_len = M_ini_kv_len(cur_ini, key);
            for (j=0; j<val_len; j++) {
                M_ini_kv_insert(merged_ini, key, M_ini_kv_get_direct(cur_ini, key, j));
            }
        /* 4. In orig and cur but not in new, flag handling (default remove). */
        } else if (!in_new && in_orig) {
            if ((flags == 0 && resolver != NULL && resolver(NULL, key, NULL)) || (flags & M_INI_MERGE_NEW_REMOVED_KEEP)) {
                val_len = M_ini_kv_len(cur_ini, key);
                for (j=0; j<val_len; j++) {
                    M_ini_kv_insert(merged_ini, key, M_ini_kv_get_direct(cur_ini, key, j));
                }
            }
        /* 5. In orig and new but not in cur, don't add (leave out).
         * We handle this case by adding keys without values for all keys that exist in cur, new and orig. */
        } else if (in_new && in_orig) {
            /* We'll handle setting the correct value for the key later. */
            M_ini_kv_add_key(merged_ini, key);
            M_list_str_insert(update_keys, key);
        }
    }
    M_list_str_destroy(keys);

    return update_keys;
}

static void M_ini_merger_handle_single_vals(M_ini_t *merged_ini, const M_ini_t *cur_ini, const M_ini_t *new_ini, const M_ini_t *orig_ini, const M_ini_settings_t *info, M_list_str_t *update_keys)
{
    const char             *key;
    const char             *val_cur;
    const char             *val_new;
    const char             *val_orig;
    size_t                  len;
    size_t                  i;
    M_ini_merge_conflict_t  flags;
    M_ini_merge_resolver_t  resolver;

    flags       = M_ini_settings_merger_get_conflict_flags(info);
    resolver    = M_ini_settings_merger_get_resolver(info);

    len  = M_list_str_len(update_keys);
    for (i=0; i<len; i++) {
        key = M_list_str_at(update_keys, i);
        if (M_ini_kv_len(cur_ini, key) > 1 || M_ini_kv_len(new_ini, key) > 1) {
            continue;
        }

        val_cur  = M_ini_kv_get_direct(cur_ini, key, 0);
        val_new  = M_ini_kv_get_direct(new_ini, key, 0);
        val_orig = M_ini_kv_get_direct(orig_ini, key, 0);

        /* 1. Cur and orig the same but new different, flag (default use new). */
        if (M_str_eq(val_cur, val_orig) && !M_str_eq(val_cur, val_new)) {
            if ((flags == 0 && resolver != NULL && resolver(key, val_cur, val_new)) || (flags & M_INI_MERGE_NEW_CHANGED_USE_CUR)) {
                M_ini_kv_set(merged_ini, key, val_cur);
            } else {
                M_ini_kv_set(merged_ini, key, val_new);
            }
        /* 2. Cur different than orig, use cur. */
        /* 3. All there are the same so keep it the same. */
        } else {
            M_ini_kv_set(merged_ini, key, val_cur);
        }
    }
}

static void M_ini_merger_handle_multi_vals(M_ini_t *merged_ini, const M_ini_t *cur_ini, const M_ini_t *new_ini, const M_ini_t *orig_ini, const M_ini_settings_t *info, M_list_str_t *update_keys)
{
    const char             *key;
    const char             *val;
    M_list_str_t           *cur_vals;
    M_list_str_t           *new_vals;
    M_list_str_t           *orig_vals;
    M_list_str_t           *merged_vals;
    size_t                  len;
    size_t                  val_len;
    size_t                  i;
    size_t                  j;
    M_ini_merge_conflict_t  flags;
    M_bool                  in_cur;
    M_bool                  in_new;
    M_bool                  in_orig;
    M_ini_merge_resolver_t  resolver;

    flags       = M_ini_settings_merger_get_conflict_flags(info);
    resolver    = M_ini_settings_merger_get_resolver(info);

    len  = M_list_str_len(update_keys);
    for (i=0; i<len; i++) {
        key = M_list_str_at(update_keys, i);
        if (M_ini_kv_len(cur_ini, key) <= 1 && M_ini_kv_len(new_ini, key) <= 1) {
            continue;
        }

        merged_vals = M_list_str_create(M_LIST_STR_NONE);

        /* Cache the values for the key for each ini so we can have faster lookup when checking if
         * the value exists. */
        cur_vals = M_list_str_create(M_LIST_STR_SORTASC);
        val_len  = M_ini_kv_len(cur_ini, key);
        for (j=0; j<val_len; j++) {
            M_list_str_insert(cur_vals, M_ini_kv_get_direct(cur_ini, key, j));
        }
        new_vals = M_list_str_create(M_LIST_STR_SORTASC);
        val_len  = M_ini_kv_len(new_ini, key);
        for (j=0; j<val_len; j++) {
            M_list_str_insert(new_vals, M_ini_kv_get_direct(new_ini, key, j));
        }
        orig_vals = M_list_str_create(M_LIST_STR_SORTASC);
        val_len   = M_ini_kv_len(orig_ini, key);
        for (j=0; j<val_len; j++) {
            M_list_str_insert(orig_vals, M_ini_kv_get_direct(orig_ini, key, j));
        }

        /* Merge the vaules for key. */
        val_len = M_ini_kv_len(cur_ini, key);
        for (j=0; j<val_len; j++) {
            val     = M_ini_kv_get_direct(cur_ini, key, j);
            in_new  = M_list_str_index_of(new_vals, val, M_LIST_STR_MATCH_VAL, NULL);
            in_orig = M_list_str_index_of(orig_vals, val, M_LIST_STR_MATCH_VAL, NULL);
            /* 1. In cur and new, keep. */
            /* 2. Only in cur, keep. */
            /* 3. In cur and orig but not in new, flag (default remove val). */
            if ((in_new)              ||
                (!in_new && !in_orig) ||
                (!in_new && in_orig &&
                    ((flags == 0 && resolver != NULL && resolver(key, val, NULL)) || (flags & M_INI_MERGE_MULTI_NEW_REMOVED_KEEP))))
            {
                M_list_str_insert(merged_vals, val);
            }
        }
        val_len = M_ini_kv_len(new_ini, key);
        for (j=0; j<val_len; j++) {
            val     = M_ini_kv_get_direct(new_ini, key, j);
            in_cur  = M_list_str_index_of(cur_vals, val, M_LIST_STR_MATCH_VAL, NULL);
            in_orig = M_list_str_index_of(orig_vals, val, M_LIST_STR_MATCH_VAL, NULL);
            /* 4. In new but not in cur or orig, add. */
            if (!in_cur && !in_orig) {
                M_list_str_insert(merged_vals, val);
            }
        }

        /* Set the values for the key. */
        val_len = M_list_str_len(merged_vals);
        for (j=0; j<val_len; j++) {
            M_ini_kv_insert(merged_ini, key, M_list_str_at(merged_vals, j));
        }

        M_list_str_destroy(orig_vals);
        M_list_str_destroy(new_vals);
        M_list_str_destroy(cur_vals);
        M_list_str_destroy(merged_vals);
    }
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_t *M_ini_merge(const M_ini_t *cur_ini, const M_ini_t *new_ini, const M_ini_t *orig_ini, const M_ini_settings_t *info)
{
    M_ini_t      *merged_ini;
    M_list_str_t *update_keys;

    if (cur_ini == NULL || new_ini == NULL || orig_ini == NULL)
        return NULL;

    merged_ini = M_ini_merger_create_merged_ini(new_ini);
    /* 1. Update keys. */
    update_keys = M_ini_merger_handle_keys(merged_ini, cur_ini, new_ini, orig_ini, info);
    /* 2. Update vals. */
    M_ini_merger_handle_single_vals(merged_ini, cur_ini, new_ini, orig_ini, info, update_keys);
    /* 3. Update multi-vals. */
    M_ini_merger_handle_multi_vals(merged_ini, cur_ini, new_ini, orig_ini, info, update_keys);

    M_list_str_destroy(update_keys);

    return merged_ini;
}
