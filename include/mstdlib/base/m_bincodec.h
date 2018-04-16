/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_BINCODEC_H__
#define __M_BINCODEC_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_bincodec Binary Conversion
 *  \ingroup mstdlib_base
 *
 * Text to binary and binary to text conversion. Supports conversion of 
 * binary to or from base64 or hex.
 *
 * Example:
 *
 * \code{.c}
 *     const char *data = "abcd";
 *     char       *out[16];
 *     char       *odata;
 *
 *     M_bincodec_encode(out, sizeof(out), (M_uint8 *)data, M_str_len(data), 0, M_BINCODEC_HEX);
 *     M_printf("hex='%s'\n", out);
 *
 *     M_bincodec_encode(out, sizeof(out), (M_uint8 *)data, M_str_len(data), 0, M_BINCODEC_BASE64);
 *     M_printf("b64='%s'\n", out);
 *
 *     odata = (char *)M_bincodec_decode_alloc(out, M_str_len(out), NULL, M_BINCODEC_BASE64ORHEX);
 *     M_printf("original='%s'\n", odata);
 *
 *     M_free(odata);
 * \endcode
 *
 * @{
 */

#define M_BINCODEC_PEM_LINE_LEN 64

/*! Binary conversion types. */
typedef enum {
	M_BINCODEC_BASE64,     /*!< Base64 encoding. */
	M_BINCODEC_HEX,        /*!< Hex encoding. */
	M_BINCODEC_BASE64ORHEX /*!< Auto detected between base64 and hex encoding. */
} M_bincodec_codec_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Encode
 */

/*! The maximum number of bytes necessary to encode using the specified codec.
 *
 * \param[in] inLen Number of bytes that will be passed to the encoder.
 * \param[in] wrap  The maximum length of a given line. Longer lines will be split with a new line. Pass 0 if
 *                  line splitting is not desired.
 * \param[in] codec The binary codec that will be used for encoding.
 *
 * \return The maximum number of bytes needed to store the data after encoding.
 */
M_API size_t M_bincodec_encode_size(size_t inLen, size_t wrap, M_bincodec_codec_t codec);


/*! Encodes data passed into it using the specified binary codec.
 *
 * \param[in] in    The binary data to encode.
 * \param[in] inLen The length of the input data.
 * \param[in] codec The binary codec to use for encoding.
 * \param[in] wrap  The maximum length of a given line. Longer lines will be split with a new line. Pass 0 if
 *                  line splitting is not desired.
 *
 * \return A new NULL terminated string with the encoded data. Otherwise NULL on error.
 *
 * \see M_free
 */
M_API char *M_bincodec_encode_alloc(const M_uint8 *in, size_t inLen, size_t wrap, M_bincodec_codec_t codec) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Encodes data passed into it using the specified binary codec.
 *
 * \param[out] out    A buffer large enought to hold the encoded data.
 * \param[in]  outLen The length of the output buffer.
 * \param[in]  in     The binary data to encode.
 * \param[in]  inLen  The length of the input data.
 * \param[in]  codec  The binary codec to use for encoding.
 * \param[in]  wrap   The maximum length of a given line. Longer lines will be split with a new line. Pass 0 if
 *                    line splitting is not desired.
 *
 * \return The number of bytes written into the output buffer.
 */
M_API size_t M_bincodec_encode(char *out, size_t outLen, const M_uint8 *in, size_t inLen, size_t wrap, M_bincodec_codec_t codec);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Decode
 */


/*! The maximum number of bytes necessary to encode using the specified codec.
 *
 * \param[in] inLen Number of bytes that will be passed to the decoder.
 * \param[in] codec The binary codec that will be used for decoding.
 *
 * \return The maximum number of bytes needed to store the data after decoding.
 */
M_API size_t M_bincodec_decode_size(size_t inLen, M_bincodec_codec_t codec);

/*! Decodes data passed into it using the specified binary codec.
 *
 * \param[in]  in     The string data to decode.
 * \param[in]  inLen  The length of the input data.
 * \param[out] outLen The length ouf the buffer.
 * \param[in]  codec  The binary codec to use for decoding.
 *
 * \return A new array with the decoded data. Otherwise NULL on error.
 *
 * \see M_free
 */
M_API M_uint8 *M_bincodec_decode_alloc(const char *in, size_t inLen, size_t *outLen, M_bincodec_codec_t codec) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Convenience function for validating decoded data is a string.  Some protocols
 *  may encode string data even if it is not necessary, so this function helps
 *  validate that data is really in string form to prevent issues.
 *
 *  \param[in]  in     The string data to decode.
 *  \param[in]  codec  The binary codec to use for decoding.
 *  \return A new buffer with decoded data, otherwise NULL on error.
 */
M_API char *M_bincodec_decode_str_alloc(const char *in, M_bincodec_codec_t codec);


/*! Decodes data passed into it using the specified binary codec.
 *
 * \param[in]  in     The string data to decode.
 * \param[in]  inLen  The length of the input data.
 * \param[out] out    A buffer large enought to hold the decoded data.
 * \param[in]  outLen The length of the output buffer.
 * \param[in]  codec  The binary codec to use for decoding.
 *
 * \return The number of bytes written into the output buffer.
 */
M_API size_t M_bincodec_decode(M_uint8 *out, size_t outLen, const char *in, size_t inLen, M_bincodec_codec_t codec);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Conversion
 */

/*! Convert a string from one binary encoding to another.
 *
 * A conversion will always be performed even when using the same input and
 * output codecs.
 *
 * \param[in] in       The data to convert.
 * \param[in] inLen    The length of the input data.
 * \param[in] wrap     The maximum length of a given line. Longer lines will be split with a new line. Pass 0 if
 *                     line splitting is not desired.
 * \param[in] inCodec  The format the input data is encoded using.
 * \param[in] outCodec The output format to convert into.
 * 
 * \return A new NULL terminated string with the converted data. Otherwise NULL on error.
 *
 * \see M_free
 */
M_API char *M_bincodec_convert_alloc(const char *in, size_t inLen, size_t wrap, M_bincodec_codec_t inCodec, M_bincodec_codec_t outCodec) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Convert a string from one binary encoding to another.
 *
 * A conversion will always be performed even when using the same input and
 * output codecs.
 *
 * \param[out] out      A buffer large enought to hold the converted data.
 * \param[in]  outLen   The length of the output buffer.
 * \param[in]  wrap     The maximum length of a given line. Longer lines will be split with a new line. Pass 0 if
 *                      line splitting is not desired.
 * \param[in]  in       The data to convert.
 * \param[in]  inLen    The length of the input data.
 * \param[in]  inCodec  The format the input data is encoded using.
 * \param[in]  outCodec The output format to convert into.
 * 
 * \return The number of bytes written into the output buffer.
 */
M_API size_t M_bincodec_convert(char *out, size_t outLen, size_t wrap, M_bincodec_codec_t outCodec, const char *in, size_t inLen, M_bincodec_codec_t inCodec);

/*! @} */

__END_DECLS

#endif /* __M_BINCODEC_H__ */
