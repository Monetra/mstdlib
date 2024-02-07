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

/* Similar to M_str_explode_str_quoted but with specific enhancements for ini */
static char **M_ini_explode_lines(const char *s, const M_ini_settings_t *info, size_t *num)
{
    size_t          i;
    size_t          s_len;
    size_t          num_strs  = 0;
    size_t          beginsect = 0;
    char          **out       = NULL;
    char           *dupstr    = NULL;
    M_bool          on_quote;
    M_bool          on_comment;
    unsigned char   delim_char;
    unsigned char   quote_char;
    unsigned char   escape_char;
    unsigned char   comment_char;

    *num = 0;
    if (s == NULL || *s == '\0')
        return NULL;

    delim_char   = M_ini_settings_get_element_delim_char(info);
    quote_char   = M_ini_settings_get_quote_char(info);
    escape_char  = M_ini_settings_get_escape_char(info);
    comment_char = M_ini_settings_get_comment_char(info);

    /* Duplicate string */
    dupstr = M_strdup(s);
    s_len  = M_str_len(dupstr);

    /* Count number of delimiters to get number of output sections */
    on_quote   = M_FALSE;
    on_comment = M_FALSE;
    for (i=0; i<s_len; i++) {
        /* Only key/values can span multiple lines by quoting. A comment cannot be quoted to span multiple lines. */
        if (quote_char != 0 && dupstr[i] == quote_char && !on_comment) {
            /* Doubling the quote char acts as escaping if the escape and quote character are the same */
            if (quote_char == escape_char && dupstr[i+1] == quote_char) {
                i++;
                continue;
            /* If the escape character preceeds the quote then it's escaped */
            } else if (quote_char != escape_char && i > 0 && dupstr[i-1] == escape_char) {
                continue;
            } else if (on_quote) {
                on_quote = M_FALSE;
            } else {
                on_quote = M_TRUE;
            }
            on_comment   = M_FALSE;
        }
        /* We're only in a comment when we encounter a comment char that isn't quoted. */
        if (comment_char != 0 && dupstr[i] == comment_char && !on_quote) {
            on_comment = M_TRUE;
        }
        if (dupstr[i] == delim_char && !on_quote) {
            num_strs++;
            beginsect  = i+1;
            on_comment = M_FALSE;
        }
    }
    if (beginsect < s_len)
        num_strs++;

    /* Create the array to hold our exploded parts */
    *num = num_strs;
    out = M_malloc(num_strs * sizeof(*out));

    /* Fill in the array with pointers to the exploded parts */
    num_strs   = 0;
    beginsect  = 0;
    on_quote   = M_FALSE;
    on_comment = M_FALSE;
    for (i=0; i<s_len; i++) {
        /* Only key/values can span multiple lines by quoting. A comment cannot be quoted to span multiple lines. */
        if (quote_char != 0 && dupstr[i] == quote_char && !on_comment) {
            /* Doubling the quote char acts as escaping if the escape and quote character are the same */
            if (quote_char == escape_char && dupstr[i+1] == quote_char) {
                i++;
                continue;
            /* If the escape character preceeds the quote then it's escaped */
            } else if (quote_char != escape_char && i > 0 && dupstr[i-1] == escape_char) {
                continue;
            } else if (on_quote) {
                on_quote = M_FALSE;
            } else {
                on_quote = M_TRUE;
            }
            on_comment   = M_FALSE;
        }
        /* We're only in a comment when we encounter a comment char that isn't quoted. */
        if (comment_char != 0 && dupstr[i] == comment_char && !on_quote) {
            on_comment = M_TRUE;
        }
        if (dupstr[i] == delim_char && !on_quote) {
            dupstr[i]       = 0;
            out[num_strs++] = dupstr+beginsect;
            beginsect       = i+1;
            on_comment      = M_FALSE;
        }
    }
    if (beginsect < s_len) {
        out[num_strs++] = dupstr+beginsect;
    }

    /* shouldn't be possible, silence coverity */
    if (num_strs == 0)
        M_free(dupstr);

    return out;
}

static void M_ini_explode_lines_free(char **strs, size_t num)
{
    if (strs != NULL) {
        /* First pointer contains entire buffer */
        if (num > 0) M_free(strs[0]);
        M_free(strs);
    }
}

/* Determine the element time from a string. */
static M_ini_element_type_t M_ini_reader_determine_type(const char *s, const M_ini_settings_t *info)
{
    if (s == NULL || *s == '\0') {
        return M_INI_ELEMENT_TYPE_EMPTY_LINE;
    }

    if ((unsigned char)*s == M_ini_settings_get_comment_char(info)) {
        return M_INI_ELEMENT_TYPE_COMMENT;
    }

    if (*s == '[') {
        return M_INI_ELEMENT_TYPE_SECTION;
    }

    /* Assume that we have a kv at this point. */
    return M_INI_ELEMENT_TYPE_KV;
}

/* M_ini_reader_determine_type should be used to determine if this is a section before calling this function.
 * This function does not check that the input conforms to the type. */
static M_bool M_ini_reader_parse_comment(const char *line, const M_ini_settings_t *info, M_ini_element_t *elem)
{
    char   *s;
    size_t  len;

    (void)info;

    s = M_strdup(line);

    /* Remove the comment character. */
    len = M_str_len(s);
    M_mem_move(s, s+1, len-1);
    s[len-1] = '\0';
//  M_str_trim(s);

    M_ini_element_comment_set_val(elem, s);

    M_free(s);
    return M_TRUE;
}

/* M_ini_reader_determine_type should be used to determine if this is a section before calling this function.
 * This function does not check that the input conforms to the type. */
static M_bool M_ini_reader_parse_section(const char *line, const M_ini_settings_t *info, M_ini_element_t *elem)
{
    char          *s;
    char          *temp;
    size_t         len;
    size_t         i;
    unsigned char  comment_char = M_ini_settings_get_comment_char(info);
    M_bool         have_name    = M_FALSE;
    M_bool         have_comment = M_FALSE;

    (void)info;

    s = M_strdup(line);

    /* Remove the start framing ([) character. */
    len = M_str_len(s);
    M_mem_move(s, s+1, len);
    s[len-1] = '\0';
    len--;

    /* Find the end framing character (]). */
    for (i=0; i<len; i++) {
        /* Section name can't include the comment character. */
        if (s[i] == comment_char) {
            M_free(s);
            return M_FALSE;
        } else if (s[i] == ']') {
            have_name = M_TRUE;
            break;
        }
    }

    /* Verify we actually have an end. */
    if (!have_name) {
        M_free(s);
        return M_FALSE;
    }

    /* Copy off the name. */
    temp = M_strdup_max(s, i);
    M_str_trim(temp);
    M_ini_element_section_set_name(elem, temp);
    M_free(temp);

    /* Look for comment. */
    for (; i<len; i++) {
        if (s[i] == comment_char) {
            have_comment = M_TRUE;
            break;
        }
    }

    if (have_comment) {
        temp = M_strdup(s+i+1);
        M_str_trim(temp);
        M_ini_element_section_set_comment(elem, temp);
        M_free(temp);
    }

    M_free(s);
    return M_TRUE;
}

static M_bool M_ini_reader_parse_kv(const char *line, const M_ini_settings_t *info, M_ini_element_t *elem)
{
    size_t  len;
    size_t  i;
    size_t  end;
    char   **parts;
    size_t   num_parts;
    char    *key;
    char    *val;
    char    *comment;

    len = M_str_len(line);
    end = len;
    /* Figure out where the key ends. */
    for (i=0; i<len; i++) {
        if (line[i] == M_ini_settings_get_kv_delim_char(info) || line[i] == M_ini_settings_get_comment_char(info)) {
            end = i;
            break;
        }
    }
    /* copy the key into the key store. */
    key = M_malloc(end+1);
    M_mem_copy(key, line, end);
    key[end] = '\0';
    M_str_trim(key);
    M_ini_element_kv_set_key(elem, key);
    M_free(key);

    /* Nothing left, we only had a key. */
    if (end == len) {
        return M_TRUE;
    }

    /* If we stopped on a comment char then the only thing left is a comment. */
    if (line[end] == M_ini_settings_get_comment_char(info)) {
        comment = M_malloc(len-end+1);
        M_mem_copy(comment, line+end+1, len-end-1);
        comment[len-end-1] = '\0';
        //M_str_trim(comment);
        M_ini_element_kv_set_comment(elem, comment);
        M_free(comment);
        return M_TRUE;
    }

    /* At this point we'll have either a value or a value and comment. The value can be quoted and have the
     * comment char in it so we need to account for the quoting. */
    parts = M_str_explode_str_quoted(M_ini_settings_get_comment_char(info), line+end+1, M_ini_settings_get_quote_char(info), M_ini_settings_get_escape_char(info), 2, &num_parts);
    /* No parts means we had key= with nothing after. We want to preserve this by using an empty string as the val. */
    if (num_parts == 0) {
        M_ini_element_kv_set_val(elem, "");
    }
    /* First part is val. */
    if (num_parts >= 1) {
        val = M_str_unquote(M_strdup(M_str_safe(parts[0])), M_ini_settings_get_quote_char(info), M_ini_settings_get_escape_char(info));
        M_ini_element_kv_set_val(elem, val);
        M_free(val);
    }
    /* Second part is comment. */
    if (num_parts >= 2) {
        comment = M_str_trim(M_strdup(M_str_safe(parts[1])));
        M_ini_element_kv_set_comment(elem, comment);
        M_free(comment);
    }
    M_str_explode_free(parts, num_parts);

    return M_TRUE;
}

static M_fs_error_t M_ini_reader_parse_line(M_ini_element_t **elem, const char *s, const M_ini_settings_t *info)
{
    M_ini_element_type_t  type = M_INI_ELEMENT_TYPE_UNKNOWN;
    char                 *line;
    M_bool                ret  = M_TRUE;

    if (elem == NULL) {
        return M_FS_ERROR_INVALID;
    }
    *elem = NULL;

    line = M_strdup(s);
    M_str_trim(line);

    type = M_ini_reader_determine_type(line, info);
    if (type == M_INI_ELEMENT_TYPE_UNKNOWN) {
        return M_FS_ERROR_GENERIC;
    }

    *elem = M_ini_element_create(type);
    switch (type) {
        case M_INI_ELEMENT_TYPE_COMMENT:
            ret = M_ini_reader_parse_comment(line, info, *elem);
            break;
        case M_INI_ELEMENT_TYPE_EMPTY_LINE:
            break;
        case M_INI_ELEMENT_TYPE_SECTION:
            ret = M_ini_reader_parse_section(line, info, *elem);
            break;
        case M_INI_ELEMENT_TYPE_KV:
            ret = M_ini_reader_parse_kv(line, info, *elem);
            break;
        case M_INI_ELEMENT_TYPE_UNKNOWN:
            /* should never get here, handled previously */
            ret = M_FALSE;
            break;
    }
    M_free(line);

    if (!ret) {
        M_ini_element_destroy(*elem);
        *elem = NULL;
        return M_FS_ERROR_GENERIC;
    }
    return M_FS_ERROR_SUCCESS;
}

/* Counts the number of actual lines (delims) in the pseudo line. The pseudo line itself is
 * one line. The count of delims in the line (quoted key values) are added to the count. */
static size_t M_ini_count_lines(const char *line, const M_ini_settings_t *info)
{
    size_t cnt = 1;
    size_t len;
    size_t i;

    len = M_str_len(line);
    for (i=0; i<len; i++) {
        if (line[i] == M_ini_settings_get_element_delim_char(info)) {
            cnt++;
        }
    }
    return cnt;
}

/* section_name should be in internal format. */
static M_bool M_ini_reader_parse_str_handle_section(const char *section_name, const M_ini_settings_t *info, M_ini_t *ini, M_ini_element_t *elem, M_ini_elements_t *section)
{
    const char *name;

    (void)info;
    (void)section;

    name = M_ini_element_section_get_name(elem);

    /* Don't add the section if it already exists. */
    if (M_ini_section_get(ini, name, NULL)) {
        M_ini_element_destroy(elem);
    } else {
        M_ini_section_insert(ini, name);
        /* Change the name to use the internal name. */
        M_ini_element_section_set_name(elem, section_name);
        M_ini_elements_insert(ini->elements, elem);
    }
    /* Already added the section (if necessary) directly to the root list of elements. */
    return M_FALSE;
}

/* KV elements need special handling.
 * 1) Construct the full name "section/key" for storing the value.
 * 2) Handle duplicates properly (collect, comment, remove...).
 *
 * section_name should be in internal format.
 */
static M_bool M_ini_reader_parse_str_handle_kv(const char *section_name, const M_ini_settings_t *info, M_ini_t *ini, M_ini_element_t *elem, M_ini_elements_t *section)
{
    M_ini_element_t *search_elem;
    char            *kv_val;
    char            *key;
    char            *int_key;
    char            *int_full_key;
    size_t           len;
    size_t           i;
    M_ini_dupkvs_t   dupkvs;

    /* Generate the internal keys. */
    key          = M_strdup(M_ini_element_kv_get_key(elem));
    int_key      = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    int_full_key = M_ini_full_key(section_name, int_key);

    /* Add the key to the lookup. */
    if (!M_hash_dict_get(ini->key_lookup, int_full_key, NULL)) {
        M_hash_dict_insert(ini->key_lookup, int_full_key, key);
    }
    M_free(key);

    /* Change the key to use the internal key. */
    M_ini_element_kv_set_key(elem, int_key);

    /* Store the value of the element for setting later. Has to be duplicated because the
     * element may be destroyed if it is a duplicate but the value still needs to be updated
     * with it's value. */
    kv_val = M_strdup(M_ini_element_kv_get_val(elem));
    dupkvs = M_ini_settings_reader_get_dupkvs_handling(info);
    /* Deal with duplicate kv elements. */
    if (M_ini_kv_has_key(ini, int_full_key)) {
        /* Modify the existing element already in the tree. */
        if (dupkvs == M_INI_DUPKVS_COMMENT_PREV || dupkvs == M_INI_DUPKVS_REMOVE_PREV) {
            len = M_ini_elements_len(section);
            for (i=0; i<len; i++) {
                search_elem = M_ini_elements_at(section, i);
                if (M_ini_element_get_type(search_elem) == M_INI_ELEMENT_TYPE_KV && M_str_eq(M_ini_element_kv_get_key(search_elem), int_key)) {
                    if (dupkvs == M_INI_DUPKVS_COMMENT_PREV) {
                        /* Comment the existing element. */
                        M_ini_element_kv_to_comment(search_elem, info);
                    } else if (dupkvs ==  M_INI_DUPKVS_REMOVE_PREV) {
                        /* Remove the existing element. */
                        M_ini_elements_remove_at(section, i);
                    }
                    break;
                }
            }
            /* Modify the new element. */
        } else if (dupkvs == M_INI_DUPKVS_COMMENT || dupkvs == M_INI_DUPKVS_REMOVE) {
            if (dupkvs == M_INI_DUPKVS_COMMENT) {
                /* This element becomes a comment. */
                M_ini_element_kv_to_comment(elem, info);
            } else if (dupkvs ==  M_INI_DUPKVS_REMOVE) {
                /* Remove (don't add) this element. */
                M_ini_element_destroy(elem);
                elem = NULL;
            }
        }
    }

    /* handle the value.*/
    if (dupkvs == M_INI_DUPKVS_COLLECT) {
        /* Store/append the vaule for the full key. */
        M_ini_kv_insert(ini, int_full_key, kv_val);
    } else {
        /* Store/overwrite the value for the full key. */
        M_ini_kv_set(ini, int_full_key, kv_val);
    }
    M_free(kv_val);
    M_free(int_full_key);
    M_free(int_key);

    if (elem)
        return M_TRUE;
    return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_fs_error_t M_ini_reader_parse_str(M_ini_t **ini, const char *s, const M_ini_settings_t *info, M_bool ignore_whitespace,  size_t *err_line)
{
    char             **lines;
    size_t             num_lines    = 0;
    char              *section_name = NULL;
    size_t             i;
    M_ini_element_t   *elem;
    M_fs_error_t       res;
    M_ini_elements_t  *section;
    size_t             real_line_cnt = 0;
    size_t             my_err_line;
    M_bool             add_elem;
    M_ini_element_type_t type;

    if (ini == NULL || s == NULL || *s == '\0' || info == NULL) {
        return M_FS_ERROR_INVALID;
    }

    if (err_line == NULL) {
        err_line = &my_err_line;
    }

    *ini = M_ini_create(ignore_whitespace);

    /* Split the ini into lines which we can parse into elements. Lines are not literal lines but
     * delimited sections of elements. For example a kv element can include newlines if quoted. */
    lines = M_ini_explode_lines(s, info, &num_lines);
    if (lines == NULL || num_lines == 0) {
        /* Silence coverity. Shouldn't be possible that if num_lines is 0 that lines will be non-NULL */
        M_ini_explode_lines_free(lines, num_lines);
        return M_FS_ERROR_SUCCESS;
    }

    /* We're at the top level until we hit a section. Once we
     * hit a section everything after will be in a section */
    section = (*ini)->elements;
    for (i=0; i<num_lines; i++) {
        /* Try to parse the line. */
        real_line_cnt += M_ini_count_lines(lines[i], info);
        res            = M_ini_reader_parse_line(&elem, lines[i], info);
        if (res != M_FS_ERROR_SUCCESS) {
            *err_line = real_line_cnt;
            M_ini_explode_lines_free(lines, num_lines);
            M_ini_destroy(*ini);
            *ini = NULL;
            return M_FS_ERROR_GENERIC;
        }

        /* Add the elements to our tree where elements after a section are inserted into
         * the section's branch. Sections are always inserted at the root level. */
        type = M_ini_element_get_type(elem);
        if (type == M_INI_ELEMENT_TYPE_SECTION) {
            /* We're on a new section to clear and set the section name.
             * The section name can't be a const reference because we may destroy the
             * section elem if it already exists in the ini. */
            M_free(section_name);
            section_name = M_ini_internal_key(M_strdup(M_ini_element_section_get_name(elem)), (*ini)->ignore_whitespace);
            M_ini_reader_parse_str_handle_section(section_name, info, *ini, elem, NULL);
            /* We're guaranteed to have the section in the ini at this point. */
            section = M_ini_section_get_direct(*ini, section_name);
        } else {
            add_elem = M_TRUE;
            if (type == M_INI_ELEMENT_TYPE_KV) {
                add_elem = M_ini_reader_parse_str_handle_kv(section_name, info, *ini, elem, section);
            }
            /* Add the element to the tree under the current section. */
            if (add_elem) {
                M_ini_elements_insert(section, elem);
            }
        }
    }

    M_free(section_name);
    M_ini_explode_lines_free(lines, num_lines);

    return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_t *M_ini_read(const char *s, const M_ini_settings_t *info, M_bool ignore_whitespace, size_t *err_line)
{
    M_ini_t      *ini = NULL;
    M_fs_error_t  res;

    res = M_ini_reader_parse_str(&ini, s, info, ignore_whitespace, err_line);
    if (res != M_FS_ERROR_SUCCESS) {
        M_ini_destroy(ini);
        return NULL;
    }

    return ini;
}

M_ini_t *M_ini_read_file(const char *path, const M_ini_settings_t *info, M_bool ignore_whitespace, size_t *err_line, size_t max_read)
{
    char         *buf = NULL;
    M_ini_t      *ini = NULL;
    M_fs_error_t  res;

    res = M_fs_file_read_bytes(path, max_read, (unsigned char **)&buf, NULL);
    if (res != M_FS_ERROR_SUCCESS || buf == NULL) {
        M_free(buf);
        return NULL;
    }

    ini = M_ini_read(buf, info, ignore_whitespace, err_line);
    M_free(buf);
    return ini;
}

