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

static int M_utf8_compar_cp(const void *arg1, const void *arg2, void *thunk)
{
    M_uint32 i1 = 0;
    M_uint32 i2 = 0;

    (void)thunk;

    if (arg1 != NULL)
        i1 = *((M_uint32 const *)arg1);
    if (arg2 != NULL)
        i2 = *((M_uint32 const *)arg2);

    if (i1 == i2)
        return 0;
    else if (i1 < i2)
        return -1;
    return 1;
}

static M_bool is_x_chr(const char *str, const char **next, M_bool(*cp_func)(M_uint32))
{
    M_uint32 cp;

    if (M_str_isempty(str))
        return M_TRUE;

    if (M_utf8_get_cp(str, &cp, next) != M_UTF8_ERROR_SUCCESS)
        return M_FALSE;

    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    return cp_func(cp);
}

static M_bool is_x(const char *str, M_bool(*cp_func)(M_uint32 cp))
{
    M_uint32 cp;

    if (M_str_isempty(str))
        return M_TRUE;

    do {
        const char *next = NULL;

        if (M_utf8_get_cp(str, &cp, &next) != M_UTF8_ERROR_SUCCESS)
            return M_FALSE;
        str = next;

        if (!M_utf8_is_valid_cp(cp))
            return M_FALSE;

        if (!cp_func(cp)) {
            return M_FALSE;
        }
    } while (*str != '\0');

    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_islower_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_sort_binary_search(M_utf8_table_Ll, M_utf8_table_Ll_len, sizeof(*M_utf8_table_Ll), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_islower_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_islower_cp);
}

M_bool M_utf8_islower(const char *str)
{
    return is_x(str, M_utf8_islower_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_isupper_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_sort_binary_search(&M_utf8_table_Lu, M_utf8_table_Lu_len, sizeof(*M_utf8_table_Lu), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isupper_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isupper_cp);
}

M_bool M_utf8_isupper(const char *str)
{
    return is_x(str, M_utf8_isupper_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_isalpha_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_utf8_islower_cp(cp))
        return M_TRUE;

    if (M_utf8_isupper_cp(cp))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Lt, M_utf8_table_Lt_len, sizeof(*M_utf8_table_Lt), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Lm, M_utf8_table_Lm_len, sizeof(*M_utf8_table_Lm), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Lo, M_utf8_table_Lo_len, sizeof(*M_utf8_table_Lo), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Nl, M_utf8_table_Nl_len, sizeof(*M_utf8_table_Nl), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isalpha_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isalpha_cp);
}

M_bool M_utf8_isalpha(const char *str)
{
    return is_x(str, M_utf8_isalpha_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_isalnum_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_utf8_isalpha_cp(cp))
        return M_TRUE;

    if (M_utf8_isnum_cp(cp))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isalnum_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isalnum_cp);
}

M_bool M_utf8_isalnum(const char *str)
{
    return is_x(str, M_utf8_isalnum_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_isnum_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_sort_binary_search(&M_utf8_table_Nd, M_utf8_table_Nd_len, sizeof(*M_utf8_table_Nd), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Nl, M_utf8_table_Nl_len, sizeof(*M_utf8_table_Nl), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_No, M_utf8_table_No_len, sizeof(*M_utf8_table_No), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isnum_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isnum_cp);
}

M_bool M_utf8_isnum(const char *str)
{
    return is_x(str, M_utf8_isnum_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_iscntrl_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_sort_binary_search(&M_utf8_table_Cc, M_utf8_table_Cc_len, sizeof(*M_utf8_table_Cc), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_iscntrl_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_iscntrl_cp);
}

M_bool M_utf8_iscntrl(const char *str)
{
    return is_x(str, M_utf8_iscntrl_cp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_utf8_ispunct_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    if (M_sort_binary_search(&M_utf8_table_Pc, M_utf8_table_Pc_len, sizeof(*M_utf8_table_Pc), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Pd, M_utf8_table_Pd_len, sizeof(*M_utf8_table_Pd), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Ps, M_utf8_table_Ps_len, sizeof(*M_utf8_table_Ps), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Pe, M_utf8_table_Pe_len, sizeof(*M_utf8_table_Pe), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Pi, M_utf8_table_Pi_len, sizeof(*M_utf8_table_Pi), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Pf, M_utf8_table_Pf_len, sizeof(*M_utf8_table_Pf), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Po, M_utf8_table_Po_len, sizeof(*M_utf8_table_Po), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_ispunct_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_ispunct_cp);
}

M_bool M_utf8_ispunct(const char *str)
{
    return is_x(str, M_utf8_ispunct_cp);
}

M_bool M_utf8_isprint_cp(M_uint32 cp)
{
    if (!M_utf8_is_valid_cp(cp))
        return M_FALSE;

    /* Optimize for ASCII range because it's most common. */
    if ((cp >= '!' && cp < 0x7f) || ( cp >= '\t' && cp <= '\r') /* \t, \n, \v, \f, \r */ || cp == ' ')
        return M_TRUE;

    /* L, N */
    if (M_utf8_isalnum_cp(cp))
        return M_TRUE;

    /* P */
    if (M_utf8_ispunct_cp(cp))
        return M_TRUE;

    /* M */
    if (M_sort_binary_search(&M_utf8_table_Mc, M_utf8_table_Mc_len, sizeof(*M_utf8_table_Mc), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Me, M_utf8_table_Me_len, sizeof(*M_utf8_table_Me), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Mn, M_utf8_table_Mn_len, sizeof(*M_utf8_table_Mn), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    /* S */
    if (M_sort_binary_search(&M_utf8_table_Sc, M_utf8_table_Sc_len, sizeof(*M_utf8_table_Sc), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Sk, M_utf8_table_Sk_len, sizeof(*M_utf8_table_Sk), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_Sm, M_utf8_table_Sm_len, sizeof(*M_utf8_table_Sm), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_sort_binary_search(&M_utf8_table_So, M_utf8_table_So_len, sizeof(*M_utf8_table_So), &cp, M_FALSE, M_utf8_compar_cp, NULL, NULL))
        return M_TRUE;

    if (M_utf8_isunihan_cp(cp))
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isprint_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isprint_cp);
}

M_bool M_utf8_isprint(const char *str)
{
    return is_x(str, M_utf8_isprint_cp);
}

M_bool M_utf8_isunihan_cp(M_uint32 cp)
{
    /* CJK Unified Ideographs Extension A */
    if (cp >= 0x3400 && cp <= 0x4DBF)
        return M_TRUE;

    /* CJK Unified Ideographs */
    if (cp >= 0x4E00 && cp <= 0x9FFF)
        return M_TRUE;

    /* CJK Compatibility Ideographs */
    if (cp >= 0xF900 && cp <= 0xFA6D)
        return M_TRUE;

    if (cp >= 0xFA70 && cp <= 0xFAD9)
        return M_TRUE;

    /* CJK Unified Ideographs Extension B */
    if (cp >= 0x20000 && cp <= 0x2A6DF)
        return M_TRUE;

    /* CJK Unified Ideographs Extension C */
    if (cp >= 0x2A700 && cp <= 0x2B738)
        return M_TRUE;

    /* CJK Unified Ideographs Extension D */
    if (cp >= 0x2B740 && cp <= 0x2B81D)
        return M_TRUE;

    /* CJK Unified Ideographs Extension E */
    if (cp >= 0x2B820 && cp <= 0x2CEA1)
        return M_TRUE;

    /* CJK Unified Ideographs Extension F */
    if (cp >= 0x2CEB0 && cp <= 0x2EBE0)
        return M_TRUE;

    /* CJK Compatibility Supplement */
    if (cp >= 0x2F800 && cp <= 0x2FA1D)
        return M_TRUE;

    /* CJK Unified Ideographs Extension G */
    if (cp >= 0x30000 && cp <= 0x3134A)
        return M_TRUE;

    return M_FALSE;
}

M_bool M_utf8_isunihan_chr(const char *str, const char **next)
{
    return is_x_chr(str, next, M_utf8_isunihan_cp);
}

M_bool M_utf8_isunihan(const char *str)
{
    return is_x(str, M_utf8_isunihan_cp);
}
