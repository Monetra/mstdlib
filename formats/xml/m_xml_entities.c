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
#include "xml/m_xml_entities.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    char        ent;
    const char *encoded;
    size_t      encoded_len;
} M_xml_entity_t;

static M_xml_entity_t M_xml_encode_entities[] = {
    { '"',  "&quot;", 6 },
    { '\'', "&apos;", 6 },
    { '&',  "&amp;",  5 },
    { '>',  "&gt;",   4 },
    { '<',  "&lt;",   4 },
    { 0,    NULL,     0 }
};
static M_xml_entity_t M_xml_decode_entities[] = {
    { '"',  "&quot;", 6 },
    { '\'', "&apos;", 6 },
    { '&',  "&amp;",  5 },
    { '>',  "&gt;",   4 },
    { '<',  "&lt;",   4 },
    { 0xA,  "&#xA;",  5 },
    { 0xD,  "&#xD;",  5 },
    { 0,    NULL,     0 }
};
static M_xml_entity_t M_xml_attribute_entities[] = {
    { '"', "&quot;", 6 },
    { '&', "&amp;",  5 },
    { '<', "&lt;",   4 },
    { 0,   NULL,     0 }
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static char *M_xml_entities_encode_int(const char *str, size_t len, M_xml_entity_t *entities)
{
    size_t   i;
    size_t   j;
    char    *ret  = NULL;
    size_t   ret_len;
    M_bool   is_first;
    size_t   prev = 0;
    M_buf_t *buf;

    buf = M_buf_create();

    is_first = M_true;
    for (i=0; i<len; i++) {
        for (j=0; entities[j].ent != 0; j++) {
            if (str[i] == entities[j].ent) {
                break;
            }
        }
        if (entities[j].ent != 0) {
            /* Hit an XML entity, flush out any previous non-xml entities to the buffer,
             * then append the XML entity */
            if (!is_first) {
                M_buf_add_bytes(buf, str+prev, i-prev);
                is_first = M_true;
            }
            M_buf_add_bytes(buf, entities[j].encoded, entities[j].encoded_len);
        } else {
            /* Not an XML entity, if there were no prior non-entities hit, cache the current position */
            if (is_first) {
                prev = i;
                is_first = M_false;
            }
        }
    }

    /* If there are cached non-xml entities, flush them out */
    if (!is_first) {
        M_buf_add_bytes(buf, str+prev, i-prev);
    }

    ret = M_buf_finish_str(buf, &ret_len);
    if (ret_len == 0)
        ret = M_strdup("");

    return ret;
}

static char *M_xml_entities_decode_int(const char *str, size_t len, M_xml_entity_t *entities)
{
    size_t i;
    size_t j;
    int cnt = 0;
    char *ret = NULL;
    ret = M_malloc(len + 1);

    for (i=0; i<len; i++) {
        if (str[i] == '&') {
            for (j=0; entities[j].ent != 0; j++) {
                if (M_str_caseeq_max(str+i, entities[j].encoded, entities[j].encoded_len)) {
                    break;
                }
            }
            if (entities[j].ent != 0) {
                ret[cnt++] = entities[j].ent;
                i+=entities[j].encoded_len-1;
            } else {
                ret[cnt++] = str[i];
            }
        } else {
            ret[cnt++] = str[i];
        }
    }

    ret[cnt] = 0;
    return ret;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_xml_entities_encode(const char *str, size_t len)
{
    return M_xml_entities_encode_int(str, len, M_xml_encode_entities);
}

char *M_xml_entities_decode(const char *str, size_t len)
{
    return M_xml_entities_decode_int(str, len, M_xml_decode_entities);
}

char *M_xml_attribute_encode(const char *str, size_t len)
{
    return M_xml_entities_encode_int(str, len, M_xml_attribute_entities);
}

char *M_xml_attribute_decode(const char *str, size_t len)
{
    return M_xml_entities_decode_int(str, len, M_xml_attribute_entities);
}
