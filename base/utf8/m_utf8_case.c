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

#include "m_utf8_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int M_utf8_compar_cp_map(const void *arg1, const void *arg2, void *thunk)
{
    const M_utf8_cp_map_t *i1 = (M_utf8_cp_map_t const *)arg1;
    const M_utf8_cp_map_t *i2 = (M_utf8_cp_map_t const *)arg2;

    (void)thunk;

    if (i1->cp1 == i2->cp1)
        return 0;
    else if (i1->cp1 < i2->cp1)
        return -1;
    return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_utf8_error_t M_utf8_toupper_cp(M_uint32 cp, M_uint32 *upper_cp)
{
    M_utf8_cp_map_t tmap = { cp, 0 };
    size_t          idx = 0;

    if (upper_cp == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;
    *upper_cp = cp;

    if (!M_utf8_is_valid_cp(cp))
        return M_UTF8_ERROR_BAD_CODE_POINT;

    /* Not found means it's an upper or not one that maps to a lower. */
    if (!M_sort_binary_search(M_utf8_table_lowtoup, M_utf8_table_lowtoup_len, sizeof(*M_utf8_table_lowtoup), &tmap, M_FALSE, M_utf8_compar_cp_map, NULL, &idx))
        return M_UTF8_ERROR_SUCCESS;

    *upper_cp = M_utf8_table_lowtoup[idx].cp2;
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_toupper_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next)
{
    M_uint32        cp       = 0;
    M_uint32        upper_cp = 0;
    M_utf8_error_t  res;

    res = M_utf8_get_cp(str, &cp, next);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    if (buf == NULL || buf_size == 0)
        return M_UTF8_ERROR_SUCCESS;

    res = M_utf8_toupper_cp(cp, &upper_cp);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    return M_utf8_from_cp(buf, buf_size, len, upper_cp);
}

M_utf8_error_t M_utf8_toupper_chr_buf(const char *str, M_buf_t *buf, const char **next)
{
    char           mybuf[8] = { 0 };
    size_t         len;
    M_utf8_error_t res;

    res = M_utf8_toupper_chr(str, mybuf, sizeof(mybuf), &len, next);
    if (res == M_UTF8_ERROR_SUCCESS)
        M_buf_add_bytes(buf, mybuf, len);

    return res;
}

M_utf8_error_t M_utf8_toupper(const char *str, char **out)
{
    M_buf_t        *buf;
    M_utf8_error_t  res;

    if (out == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str)) {
        *out = M_strdup("");
        return M_UTF8_ERROR_SUCCESS;
    }

    buf = M_buf_create();
    res = M_utf8_toupper_buf(str, buf);
    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_cancel(buf);
        return res;
    }

    *out = M_buf_finish_str(buf, NULL);
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_toupper_buf(const char *str, M_buf_t *buf)
{
    const char     *p;
    M_uint32        cp;
    M_uint32        upper_cp;
    size_t          start;
    M_utf8_error_t  res;

    if (buf == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str))
        return M_UTF8_ERROR_SUCCESS;

    start = M_buf_len(buf);
    do {
        res = M_utf8_get_cp(str, &cp, &p);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_toupper_cp(cp, &upper_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_from_cp_buf(buf, upper_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        str = p;
    } while (str != NULL && *str != '\0');

    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_truncate(buf, start);
        return res;
    }

    return M_UTF8_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_utf8_error_t M_utf8_tolower_cp(M_uint32 cp, M_uint32 *lower_cp)
{
    M_utf8_cp_map_t tmap = { cp, 0 };
    size_t          idx = 0;

    if (lower_cp == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;
    *lower_cp = cp;

    if (!M_utf8_is_valid_cp(cp))
        return M_UTF8_ERROR_BAD_CODE_POINT;

    /* Not found means it's a lower or not one that maps to an upper. */
    if (!M_sort_binary_search(M_utf8_table_uptolow, M_utf8_table_uptolow_len, sizeof(*M_utf8_table_uptolow), &tmap, M_FALSE, M_utf8_compar_cp_map, NULL, &idx))
        return M_UTF8_ERROR_SUCCESS;

    *lower_cp = M_utf8_table_uptolow[idx].cp2;
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_tolower_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next)
{
    M_uint32        cp       = 0;
    M_uint32        lower_cp = 0;
    M_utf8_error_t  res;

    res = M_utf8_get_cp(str, &cp, next);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    if (buf == NULL || buf_size == 0)
        return M_UTF8_ERROR_SUCCESS;

    res = M_utf8_tolower_cp(cp, &lower_cp);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    return M_utf8_from_cp(buf, buf_size, len, lower_cp);
}

M_utf8_error_t M_utf8_tolower_chr_buf(const char *str, M_buf_t *buf, const char **next)
{
    char           mybuf[8] = { 0 };
    size_t         len;
    M_utf8_error_t res;

    res = M_utf8_tolower_chr(str, mybuf, sizeof(mybuf), &len, next);
    if (res == M_UTF8_ERROR_SUCCESS)
        M_buf_add_bytes(buf, mybuf, len);

    return res;
}

M_utf8_error_t M_utf8_tolower(const char *str, char **out)
{
    M_buf_t        *buf;
    M_utf8_error_t  res;

    if (out == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str)) {
        *out = M_strdup("");
        return M_UTF8_ERROR_SUCCESS;
    }

    buf = M_buf_create();
    res = M_utf8_tolower_buf(str, buf);
    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_cancel(buf);
        return res;
    }

    *out = M_buf_finish_str(buf, NULL);
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_tolower_buf(const char *str, M_buf_t *buf)
{
    const char     *p;
    M_uint32        cp;
    M_uint32        lower_cp;
    size_t          start;
    M_utf8_error_t  res;

    if (buf == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str))
        return M_UTF8_ERROR_SUCCESS;

    start = M_buf_len(buf);
    do {
        res = M_utf8_get_cp(str, &cp, &p);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_tolower_cp(cp, &lower_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_from_cp_buf(buf, lower_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        str = p;
    } while (str != NULL && *str != '\0');

    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_truncate(buf, start);
        return res;
    }

    return M_UTF8_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_utf8_error_t M_utf8_totitle_cp(M_uint32 cp, M_uint32 *title_cp)
{
    M_utf8_cp_map_t tmap = { cp, 0 };
    size_t          idx = 0;

    if (title_cp == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;
    *title_cp = cp;

    if (!M_utf8_is_valid_cp(cp))
        return M_UTF8_ERROR_BAD_CODE_POINT;

    /* Not found means it's a title or not one that maps to an upper. */
    if (!M_sort_binary_search(M_utf8_table_uptolow, M_utf8_table_uptolow_len, sizeof(*M_utf8_table_uptolow), &tmap, M_FALSE, M_utf8_compar_cp_map, NULL, &idx))
        return M_UTF8_ERROR_SUCCESS;

    *title_cp = M_utf8_table_uptolow[idx].cp2;
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_totitle_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next)
{
    M_uint32        cp       = 0;
    M_uint32        title_cp = 0;
    M_utf8_error_t  res;

    res = M_utf8_get_cp(str, &cp, next);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    if (buf == NULL || buf_size == 0)
        return M_UTF8_ERROR_SUCCESS;

    res = M_utf8_totitle_cp(cp, &title_cp);
    if (res != M_UTF8_ERROR_SUCCESS)
        return res;

    return M_utf8_from_cp(buf, buf_size, len, title_cp);
}

M_utf8_error_t M_utf8_totitle_chr_buf(const char *str, M_buf_t *buf, const char **next)
{
    char           mybuf[8] = { 0 };
    size_t         len;
    M_utf8_error_t res;

    res = M_utf8_totitle_chr(str, mybuf, sizeof(mybuf), &len, next);
    if (res == M_UTF8_ERROR_SUCCESS)
        M_buf_add_bytes(buf, mybuf, len);

    return res;
}

M_utf8_error_t M_utf8_totitle(const char *str, char **out)
{
    M_buf_t        *buf;
    M_utf8_error_t  res;

    if (out == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str)) {
        *out = M_strdup("");
        return M_UTF8_ERROR_SUCCESS;
    }

    buf = M_buf_create();
    res = M_utf8_totitle_buf(str, buf);
    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_cancel(buf);
        return res;
    }

    *out = M_buf_finish_str(buf, NULL);
    return M_UTF8_ERROR_SUCCESS;
}

M_utf8_error_t M_utf8_totitle_buf(const char *str, M_buf_t *buf)
{
    const char     *p;
    M_uint32        cp;
    M_uint32        title_cp;
    size_t          start;
    M_utf8_error_t  res;

    if (buf == NULL)
        return M_UTF8_ERROR_INVALID_PARAM;

    if (M_str_isempty(str))
        return M_UTF8_ERROR_SUCCESS;

    start = M_buf_len(buf);
    do {
        res = M_utf8_get_cp(str, &cp, &p);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_totitle_cp(cp, &title_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        res = M_utf8_from_cp_buf(buf, title_cp);
        if (res != M_UTF8_ERROR_SUCCESS)
            break;

        str = p;
    } while (str != NULL && *str != '\0');

    if (res != M_UTF8_ERROR_SUCCESS) {
        M_buf_truncate(buf, start);
        return res;
    }

    return M_UTF8_ERROR_SUCCESS;
}
