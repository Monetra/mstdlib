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

#ifndef __M_UTF8_INT_H__
#define __M_UTF8_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    M_uint32 cp1;
    M_uint32 cp2;
} M_utf8_cp_map_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern const M_uint32 M_utf8_table_Cc[];
extern const size_t M_utf8_table_Cc_len;

extern const M_uint32 M_utf8_table_Cf[];
extern const size_t M_utf8_table_Cf_len;

extern const M_uint32 M_utf8_table_Co[];
extern const size_t M_utf8_table_Co_len;

extern const M_uint32 M_utf8_table_Cs[];
extern const size_t M_utf8_table_Cs_len;

extern const M_uint32 M_utf8_table_Ll[];
extern const size_t M_utf8_table_Ll_len;

extern const M_uint32 M_utf8_table_Lm[];
extern const size_t M_utf8_table_Lm_len;

extern const M_uint32 M_utf8_table_Lo[];
extern const size_t M_utf8_table_Lo_len;

extern const M_uint32 M_utf8_table_Lt[];
extern const size_t M_utf8_table_Lt_len;

extern const M_uint32 M_utf8_table_Lu[];
extern const size_t M_utf8_table_Lu_len;

extern const M_uint32 M_utf8_table_Mc[];
extern const size_t M_utf8_table_Mc_len;

extern const M_uint32 M_utf8_table_Me[];
extern const size_t M_utf8_table_Me_len;

extern const M_uint32 M_utf8_table_Mn[];
extern const size_t M_utf8_table_Mn_len;

extern const M_uint32 M_utf8_table_Nd[];
extern const size_t M_utf8_table_Nd_len;

extern const M_uint32 M_utf8_table_Nl[];
extern const size_t M_utf8_table_Nl_len;

extern const M_uint32 M_utf8_table_No[];
extern const size_t M_utf8_table_No_len;

extern const M_uint32 M_utf8_table_Pc[];
extern const size_t M_utf8_table_Pc_len;

extern const M_uint32 M_utf8_table_Pd[];
extern const size_t M_utf8_table_Pd_len;

extern const M_uint32 M_utf8_table_Pe[];
extern const size_t M_utf8_table_Pe_len;

extern const M_uint32 M_utf8_table_Pf[];
extern const size_t M_utf8_table_Pf_len;

extern const M_uint32 M_utf8_table_Pi[];
extern const size_t M_utf8_table_Pi_len;

extern const M_uint32 M_utf8_table_Po[];
extern const size_t M_utf8_table_Po_len;

extern const M_uint32 M_utf8_table_Ps[];
extern const size_t M_utf8_table_Ps_len;

extern const M_uint32 M_utf8_table_Sc[];
extern const size_t M_utf8_table_Sc_len;

extern const M_uint32 M_utf8_table_Sk[];
extern const size_t M_utf8_table_Sk_len;

extern const M_uint32 M_utf8_table_Sm[];
extern const size_t M_utf8_table_Sm_len;

extern const M_uint32 M_utf8_table_So[];
extern const size_t M_utf8_table_So_len;

extern const M_uint32 M_utf8_table_Zl[];
extern const size_t M_utf8_table_Zl_len;

extern const M_uint32 M_utf8_table_Zp[];
extern const size_t M_utf8_table_Zp_len;

extern const M_uint32 M_utf8_table_Zs[];
extern const size_t M_utf8_table_Zs_len;

extern const M_utf8_cp_map_t M_utf8_table_uptolow[];
extern const size_t M_utf8_table_uptolow_len;

extern const M_utf8_cp_map_t M_utf8_table_lowtoup[];
extern const size_t M_utf8_table_lowtoup_len;

extern const M_utf8_cp_map_t M_utf8_table_title[];
extern const size_t M_utf8_table_title_len;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_UTS8_INT_H__ */
