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

#ifndef __M_ENDIAN_H__
#define __M_ENDIAN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


M_API M_uint16 M_swap16(M_uint16 n) M_WARN_UNUSED_RESULT;
M_API M_uint32 M_swap32(M_uint32 n) M_WARN_UNUSED_RESULT;
M_API M_uint64 M_swap64(M_uint64 n) M_WARN_UNUSED_RESULT;

M_BEGIN_IGNORE_REDECLARATIONS
#if M_BLACKLIST_FUNC == 1
#    ifdef htonl
#      undef htonl
#    else
    M_DEPRECATED_FOR(M_hton32, M_uint32  htonl(M_uint32  hostlong))
#    endif
#    ifdef htons
#      undef htons
#    else
    M_DEPRECATED_FOR(M_hton16, M_uint16 htons(M_uint16 hostshort))
#    endif
#    ifdef ntohl
#      undef ntohl
#    else
    M_DEPRECATED_FOR(M_ntoh32, M_uint32  ntohl(M_uint32  netlong))
#    endif
#    ifdef ntohs
#      undef ntohs
#    else
    M_DEPRECATED_FOR(M_ntoh16, M_uint16 ntohs(M_uint16 netshort))
#    endif
#  endif
M_END_IGNORE_REDECLARATIONS

/*! \addtogroup m_endian Endian
 *  \ingroup mstdlib_base
 *
 * Conversion between byte orders.
 *
 * @{
 */


/*! Endianness */
typedef enum {
    M_ENDIAN_BIG,
    M_ENDIAN_LITTLE
} M_endian_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a 16-bit unsigned integer in host byte order to network byte order.
 *
 * \param[in] h16 16-bit unsigned integer in host byte order.
 *
 * \return 16-bit unsigned integer in network byte order.
 */
M_API M_uint16 M_hton16(M_uint16 h16) M_WARN_UNUSED_RESULT;


/*! Convert a 32-bit unsigned integer in host byte order to network byte order.
 *
 * \param[in] h32 32-bit unsigned integer in host byte order.
 *
 * \return 32-bit unsigned integer in network byte order.
 */
M_API M_uint32 M_hton32(M_uint32 h32) M_WARN_UNUSED_RESULT;


/*! Convert a 64-bit unsigned integer in host byte order to network byte order.
 *
 * \param[in] h64 64-bit unsigned integer in host byte order.
 *
 * \return 64-bit unsigned integer in network byte order.
 */
M_API M_uint64 M_hton64(M_uint64 h64) M_WARN_UNUSED_RESULT;


/*! Convert a 16-bit unsigned integer in host byte order to little endian byte order.
 *
 * \param[in] h16 16-bit unsigned integer in host byte order.
 *
 * \return 16-bit unsigned integer in little endian byte order.
 */
M_API M_uint16 M_htol16(M_uint16 h16) M_WARN_UNUSED_RESULT;


/*! Convert a 32-bit unsigned integer in host byte order to little endian byte order.
 *
 * \param[in] h32 32-bit unsigned integer in host byte order.
 *
 * \return 32-bit unsigned integer in little endian byte order.
 */
M_API M_uint32 M_htol32(M_uint32 h32) M_WARN_UNUSED_RESULT;


/*! Convert a 64-bit unsigned integer in host byte order to little endian byte order.
 *
 * \param[in] h64 64-bit unsigned integer in host byte order.
 *
 * \return 64-bit unsigned integer in little endian byte order.
 */
M_API M_uint64 M_htol64(M_uint64 h64) M_WARN_UNUSED_RESULT;


/*! Convert a 16-bit unsigned integer in network byte order to host byte order.
 *
 * \param[in] be16 16-bit unsigned integer in network byte order.
 *
 * \return 16-bit unsigned integer in little host byte order.
 */
M_API M_uint16 M_ntoh16(M_uint16 be16) M_WARN_UNUSED_RESULT;


/*! Convert a 32-bit unsigned integer in network byte order to host byte order.
 *
 * \param[in] be32 32-bit unsigned integer in network byte order.
 *
 * \return 32-bit unsigned integer in little host byte order.
 */
M_API M_uint32 M_ntoh32(M_uint32 be32) M_WARN_UNUSED_RESULT;


/*! Convert a 64-bit unsigned integer in network byte order to host byte order.
 *
 * \param[in] be64 64-bit unsigned integer in network byte order.
 *
 * \return 64-bit unsigned integer in little host byte order.
 */
M_API M_uint64 M_ntoh64(M_uint64 be64) M_WARN_UNUSED_RESULT;


/*! Convert a 16-bit unsigned integer in little endian byte order to host byte order.
 *
 * \param[in] be16 16-bit unsigned integer in little endian byte order.
 *
 * \return 16-bit unsigned integer in little host byte order.
 */
M_API M_uint16 M_ltoh16(M_uint16 be16) M_WARN_UNUSED_RESULT;


/*! Convert a 32-bit unsigned integer in little endian byte order to host byte order.
 *
 * \param[in] be32 32-bit unsigned integer in little endian byte order.
 *
 * \return 32-bit unsigned integer in little host byte order.
 */
M_API M_uint32 M_ltoh32(M_uint32 be32) M_WARN_UNUSED_RESULT;


/*! Convert a 64-bit unsigned integer in little endian byte order to host byte order.
 *
 * \param[in] be64 64-bit unsigned integer in little endian byte order.
 *
 * \return 64-bit unsigned integer in little host byte order.
 */
M_API M_uint64 M_ltoh64(M_uint64 be64) M_WARN_UNUSED_RESULT;

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Legacy
 */

#  define M_BIG_ENDIAN    1
#  define M_LITTLE_ENDIAN 2

M_API int M_Current_Endian(void);

#  define M_hton_u16(n)   M_hton16(n)
#  define M_hton_16(n)    M_hton16(n)
#  define M_hton_u32(n)   M_hton32(n)
#  define M_hton_32(n)    M_hton32(n)
#  define M_hton_u64(n)   M_hton64(n)
#  define M_hton_64(n)    M_hton64(n)
#  define M_ntoh_u16(n)   M_hton_u16(n)
#  define M_ntoh_16(n)    M_hton_16(n)
#  define M_ntoh_u32(n)   M_hton_u32(n)
#  define M_ntoh_32(n)    M_hton_32(n)
#  define M_ntoh_u64(n)   M_hton_u64(n)
#  define M_ntoh_64(n)    M_hton_64(n)

#  define M_swapu16(n)    M_swap16(n)
#  define M_swapu32(n)    M_swap32(n)
#  define M_swapu64(n)    M_swap64(n)

__END_DECLS

#endif /* __M_ENDIAN_H__ */
