/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

#ifndef __M_TEXTCODEC_H__
#define __M_TEXTCODEC_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_textcodec Text Encoding Conversion
 *  \ingroup mstdlib_base
 *
 * Text codec converstion. E.g. utf-8 to utf-16.
 *
 * utf-8 is used as the base codec. Input for encode should be utf-8 and
 * output from decode will be utf-8.
 *
 * @{
 */ 


/*! Error handling logic. */
typedef enum {
	M_TEXTCODEC_EHANDLER_FAIL,    /*!< Errors should be considered a hard failure. */
	M_TEXTCODEC_EHANDLER_REPLACE, /*!< Encode replace with ?. Decode replace with U+FFFD. */
	M_TEXTCODEC_EHANDLER_IGNORE   /*!< Ignore data that cannot be encoded or decoded in the codec. */
} M_textcodec_ehandler_t;


/*! Text codecs that can be used for encoding and decoding. */
typedef enum {
	M_TEXTCODEC_UNKNOWN,         /*!< Unknown / invalid codec. */
	M_TEXTCODEC_UTF8,            /*!< Utf-8. */
	M_TEXTCODEC_ASCII,           /*!< Ascii. */
	M_TEXTCODEC_PERCENT_URL,     /*!< Percent with space as %20 for use as a URL rules. Must be utf-8. */
	M_TEXTCODEC_PERCENT_URLPLUS, /*!< Percent with space as + for use as a URL. Must be utf-8. */
	M_TEXTCODEC_PERCENT_FORM,    /*!< Percent suitable for use as form data. Must be utf-8. */
	M_TEXTCODEC_CP1252,          /*!< Windows code page 1252. */
	M_TEXTCODEC_ISO88591,        /*!<  ISO-8859-1. Latin 1. */
} M_textcodec_codec_t;


/*! Result of a codec conversion. */
typedef enum {
	M_TEXTCODEC_ERROR_SUCCESS,          /*!< Successfully converted. */
	M_TEXTCODEC_ERROR_SUCCESS_EHANDLER, /*!< Succesfully converted based on error handling logic. */
	M_TEXTCODEC_ERROR_FAIL,             /*!< Failure to convert. */
	M_TEXTCODEC_ERROR_BADINPUT,         /*!< Input not in specified encoding. This cannot always be detected and
	                                         should not be used as a means of determining input encoding. */
	M_TEXTCODEC_ERROR_INVALID_PARAM,    /*!< Invalid parameter. */
} M_textcodec_error_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Encode a utf-8 string using the requested text encoding.
 *
 * \param[out] out      Encoded string.
 * \param[in]  in       Input utf-8 string.
 * \param[in]  ehandler Error handling logic to use.
 * \param[in]  codec    Encoding to use for output.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_encode(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Encode a utf-8 string into an M_buf_t using the requested text encoding.
 *
 * \param[in] buf      Buffer to put encoded string data.
 * \param[in] in       Input utf-8 string.
 * \param[in] ehandler Error handling logic to use.
 * \param[in] codec    Encoding to use for output.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_encode_buf(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Encode a utf-8 string into an M_parser_t using the requested text encoding.
 *
 * \param[in] parser   Parser to put encoded string data.
 * \param[in] in       Input utf-8 string.
 * \param[in] ehandler Error handling logic to use.
 * \param[in] codec    Encoding to use for output.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_encode_parser(M_parser_t *parser, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Decode a string to utf-8.
 *
 * \param[out] out      utf-8 string.
 * \param[in]  in       Input encoded string.
 * \param[in]  ehandler Error handling logic to use.
 * \param[in]  codec    Encoding of the input string.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_decode(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Decode a string to utf-8 into a M_buf_t.
 *
 * \param[in] buf      Buffer to put decoded utf-8 string data.
 * \param[in] in       Input encoded string.
 * \param[in] ehandler Error handling logic to use.
 * \param[in] codec    Encoding of the input string.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_decode_buf(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Decode a string to utf-8 into a M_parser_t.
 *
 * \param[in] parser   Parser to put decoded utf-8 string data.
 * \param[in] in       Input encoded string.
 * \param[in] ehandler Error handling logic to use.
 * \param[in] codec    Encoding of the input string.
 *
 * \return Result.
 */
M_API M_textcodec_error_t M_textcodec_decode_parser(M_parser_t *parser, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Returns if error code is a failure or not.
 *
 * \param[in] err Error to evaluate
 *
 * \return M_TRUE if error, M_FALSE if not.
 */
M_API M_bool M_textcodec_error_is_error(M_textcodec_error_t err);


/*! Get the codec from the string name.
 *
 * \param[in] s Codec as a string.
 *
 * \return Codec.
 */
M_API M_textcodec_codec_t M_textcodec_codec_from_str(const char *s);


/*! Covert the codec to its string name.
 *
 * \param[in] codec Codec.
 *
 * \return String.
 */
M_API const char *M_textcodec_codec_to_str(M_textcodec_codec_t codec);

/*! @} */

__END_DECLS

#endif /* __M_TEXTCODEC_H__ */
