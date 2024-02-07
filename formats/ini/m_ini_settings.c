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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_ini_settings {
    /* Shared/General */
    unsigned char           delim_char;
    unsigned char           quote_char;
    unsigned char           escape_char;
    unsigned char           comment_char;
    unsigned char           kv_delim_char;
    M_ini_padding_t         padding;

    /* Reader specific settings. */
    M_ini_dupkvs_t          reader_dupkvs;

    /* Writer specific settings. */
    M_ini_multivals_t       writer_multivals;
    char                   *writer_line_ending;

    /* Merger specific settings. */
    M_ini_merge_conflict_t  merger_conflict_flags;
    M_ini_merge_resolver_t  merger_resolver;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_settings_t *M_ini_settings_create(void)
{
    M_ini_settings_t *info;

    info                = M_malloc_zero(sizeof(*info));
    /* Some sane defaults. */
    info->delim_char    = '\n';
    info->comment_char  = '#';
    info->kv_delim_char = '=';

    return info;
}

void M_ini_settings_destroy(M_ini_settings_t *info)
{
    if (info == NULL)
        return;
    M_free(info->writer_line_ending);
    M_free(info);
}

unsigned char M_ini_settings_get_element_delim_char(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->delim_char;
}

unsigned char M_ini_settings_get_quote_char(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->quote_char;
}

unsigned char M_ini_settings_get_escape_char(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->escape_char;
}

unsigned char M_ini_settings_get_comment_char(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->comment_char;
}

unsigned char M_ini_settings_get_kv_delim_char(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->kv_delim_char;
}

M_uint32 M_ini_settings_get_padding(const M_ini_settings_t *info)
{
    if (info == NULL)
        return M_INI_PADDING_NONE;
    return info->padding;
}

M_ini_dupkvs_t M_ini_settings_reader_get_dupkvs_handling(const M_ini_settings_t *info)
{
    if (info == NULL)
        return M_INI_DUPKVS_COMMENT_PREV;
    return info->reader_dupkvs;
}

M_ini_multivals_t M_ini_settings_writer_get_multivals_handling(const M_ini_settings_t *info)
{
    if (info == NULL)
        return M_INI_MULTIVALS_USE_LAST;
    return info->writer_multivals;
}

const char *M_ini_settings_writer_get_line_ending(const M_ini_settings_t *info)
{
    if (info == NULL)
        return NULL;
    return info->writer_line_ending;
}

M_uint32 M_ini_settings_merger_get_conflict_flags(const M_ini_settings_t *info)
{
    if (info == NULL)
        return 0;
    return info->merger_conflict_flags;
}
M_ini_merge_resolver_t M_ini_settings_merger_get_resolver(const M_ini_settings_t *info)
{
    if (info == NULL)
        return NULL;
    return info->merger_resolver;
}

void M_ini_settings_set_element_delim_char(M_ini_settings_t *info, unsigned char val)
{
    if (info == NULL)
        return;
    info->delim_char = val;
}

void M_ini_settings_set_quote_char(M_ini_settings_t *info, unsigned char val)
{
    if (info == NULL)
        return;
    info->quote_char = val;
}

void M_ini_settings_set_escape_char(M_ini_settings_t *info, unsigned char val)
{
    if (info == NULL)
        return;
    info->escape_char = val;
}

void M_ini_settings_set_comment_char(M_ini_settings_t *info, unsigned char val)
{
    if (info == NULL)
        return;
    info->comment_char = val;
}

void M_ini_settings_set_kv_delim_char(M_ini_settings_t *info, unsigned char val)
{
    if (info == NULL)
        return;
    info->kv_delim_char = val;
}

void M_ini_settings_set_padding(M_ini_settings_t *info, M_uint32 val)
{
    if (info == NULL)
        return;
    info->padding = val;
}

void M_ini_settings_reader_set_dupkvs_handling(M_ini_settings_t *info, M_ini_dupkvs_t val)
{
    if (info == NULL)
        return;
    info->reader_dupkvs = val;
}

void M_ini_settings_writer_set_multivals_handling(M_ini_settings_t *info, M_ini_multivals_t val)
{
    if (info == NULL)
        return;
    info->writer_multivals = val;
}

void M_ini_settings_writer_set_line_ending(M_ini_settings_t *info, const char *val)
{
    if (info == NULL)
        return;
    M_free(info->writer_line_ending);
    info->writer_line_ending = M_strdup(val);
}

void M_ini_settings_merger_set_conflict_flags(M_ini_settings_t *info, M_uint32 val)
{
    if (info == NULL)
        return;
    info->merger_conflict_flags = val;
}

void M_ini_settings_merger_set_resolver(M_ini_settings_t *info, M_ini_merge_resolver_t val)
{
    if (info == NULL)
        return;
    info->merger_resolver = val;
}
