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
/* TEMPORARY ADAPTER STUFF
 */

M_BEGIN_IGNORE_DEPRECATIONS

static int M_ENDIAN_TEST = 1;

int M_Current_Endian(void)
{
    return((*((char *)&M_ENDIAN_TEST))?M_LITTLE_ENDIAN:M_BIG_ENDIAN);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const unsigned int M_endian_test = 1;

static __inline__ M_endian_t M_endianess(void)
{
    return *((const M_uint8 *)&M_endian_test)
        ? M_ENDIAN_LITTLE
        : M_ENDIAN_BIG;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint16 M_swap16(M_uint16 n)
{
    return (M_uint16)
        ((n & 0x00ffU) << 8) |
            ((n & 0xff00U) >> 8);
}

M_uint32 M_swap32(M_uint32 n)
{
    return ((n & 0x000000ffU) << 24) |
               ((n & 0x0000ff00U) <<  8) |
               ((n & 0x00ff0000U) >>  8) |
               ((n & 0xff000000U) >> 24);
}

M_uint64 M_swap64(M_uint64 n)
{
    return ((n & (M_uint64)0x00000000000000ffULL) << 56) |
           ((n & (M_uint64)0x000000000000ff00ULL) << 40) |
           ((n & (M_uint64)0x0000000000ff0000ULL) << 24) |
           ((n & (M_uint64)0x00000000ff000000ULL) <<  8) |
           ((n & (M_uint64)0x000000ff00000000ULL) >>  8) |
           ((n & (M_uint64)0x0000ff0000000000ULL) >> 24) |
           ((n & (M_uint64)0x00ff000000000000ULL) >> 40) |
           ((n & (M_uint64)0xff00000000000000ULL) >> 56);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* host to network (big-endian) */
M_uint16 M_hton16(M_uint16 h16 ) { return (M_uint16)(M_endianess() == M_ENDIAN_BIG    ?  h16 : M_swap16( h16)); }
M_uint32 M_hton32(M_uint32 h32 ) { return (M_uint32)(M_endianess() == M_ENDIAN_BIG    ?  h32 : M_swap32( h32)); }
M_uint64 M_hton64(M_uint64 h64 ) { return (M_uint64)(M_endianess() == M_ENDIAN_BIG    ?  h64 : M_swap64( h64)); }

/* host to little endian */
M_uint16 M_htol16(M_uint16 h16 ) { return (M_uint16)(M_endianess() == M_ENDIAN_LITTLE ?  h16 : M_swap16( h16)); }
M_uint32 M_htol32(M_uint32 h32 ) { return (M_uint32)(M_endianess() == M_ENDIAN_LITTLE ?  h32 : M_swap32( h32)); }
M_uint64 M_htol64(M_uint64 h64 ) { return (M_uint64)(M_endianess() == M_ENDIAN_LITTLE ?  h64 : M_swap64( h64)); }

/* network (big-endian) to host */
M_uint16 M_ntoh16(M_uint16 be16) { return (M_uint16)(M_endianess() == M_ENDIAN_BIG    ? be16 : M_swap16(be16)); }
M_uint32 M_ntoh32(M_uint32 be32) { return (M_uint32)(M_endianess() == M_ENDIAN_BIG    ? be32 : M_swap32(be32)); }
M_uint64 M_ntoh64(M_uint64 be64) { return (M_uint64)(M_endianess() == M_ENDIAN_BIG    ? be64 : M_swap64(be64)); }

/* little-endian to host */
M_uint16 M_ltoh16(M_uint16 le16) { return (M_uint16)(M_endianess() == M_ENDIAN_LITTLE ? le16 : M_swap16(le16)); }
M_uint32 M_ltoh32(M_uint32 le32) { return (M_uint32)(M_endianess() == M_ENDIAN_LITTLE ? le32 : M_swap32(le32)); }
M_uint64 M_ltoh64(M_uint64 le64) { return (M_uint64)(M_endianess() == M_ENDIAN_LITTLE ? le64 : M_swap64(le64)); }

M_END_IGNORE_DEPRECATIONS
