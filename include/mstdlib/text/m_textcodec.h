/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
 *  \ingroup m_text
 *
 * Text codec conversion. E.g. utf-8 to X and X to utf-8.
 *
 * utf-8 is used as the base codec. Input for encode should be utf-8 and
 * output from decode will be utf-8.
 *
 *
 * Codec           | Name                              | Alias
 * ----------------|-----------------------------------|------
 * UTF8            | utf8                              | utf-8, utf_8
 * ASCII           | ascii                             | us-ascii
 * CP037           | cp037                             | ibm037, ibm-037, ibm039, ibm-039
 * CP500           | cp500                             | ibm500, ibm-500, ebcdic-cp-be, ebcdic-cp-ch
 * CP874           | cp874                             | windows-874
 * CP1250          | cp1250                            | windows-1250
 * CP1251          | cp1251                            | windows-1251
 * CP1252          | cp1252                            | windows-1252
 * CP1253          | cp1253                            | windows-1253
 * CP1254          | cp1254                            | windows-1254
 * CP1255          | cp1255                            | windows-1255
 * CP1256          | cp1256                            | windows-1256
 * CP1257          | cp1257                            | windows-1257
 * CP1258          | cp1258                            | windows-1258
 * ISO8859_1       | latin_1                           | latin-1, latin1, latin 1, latin, l1, iso-8859-1, iso8859-1, iso8859_1, iso88591, 8859, 88591, cp819
 * ISO8859_2       | latin_2                           | latin-2, latin2, latin 2, l2, iso-8859-2, iso8859-2, iso8859_2, iso88592, 88592
 * ISO8859_3       | latin_3                           | latin-3, latin3, latin 3, l3, iso-8859-3, iso8859-3, iso8859_3, iso88593, 88593
 * ISO8859_4       | latin_4                           | latin-4, latin4, latin 4, l4, iso-8859-4, iso8859-4, iso8859_4, iso88594, 88594
 * ISO8859_5       | cyrillic                          | iso-8859-5, iso8859-5, iso8859_5, iso88595, 88595
 * ISO8859_6       | arabic                            | iso-8859-6, iso8859-6, iso8859_6, iso88596, 88596
 * ISO8859_7       | greek                             | iso-8859-7, iso8859-7, iso8859_7, iso88597, 88597
 * ISO8859_8       | hebrew                            | iso-8859-8, iso8859-8, iso8859_8, iso88598, 88598
 * ISO8859_9       | latin_5                           | latin-5, latin5, latin 5, l5, iso-8859-9, iso8859-9, iso8859_9, iso88599, 88599
 * ISO8859_10      | latin_6                           | latin-6, latin6, latin 6, l6, iso-8859-10, iso8859-10, iso8859_10, iso885910, 885910
 * ISO8859_11      | thai                              | iso-8859-11, iso8859-11, iso8859_11, iso885911, 885911
 * ISO8859_13      | latin_7                           | latin-7, latin7, latin 7, l7, iso-8859-13, iso8859-13, iso8859_13, iso885913, 885913
 * ISO8859_14      | latin_8                           | latin-8, latin8, latin 8, l8, iso-8859-14, iso8859-14, iso8859_14, iso885914, 885914
 * ISO8859_15      | latin_9                           | latin-9, latin9, latin 9, l9, iso-8859-15, iso8859-15, iso8859_15, iso885915, 885915
 * ISO8859_16      | latin_10                          | latin-10, latin10, latin 10, l10, iso-8859-16, iso8859-16, iso8859_16, iso885916, 885916
 * PERCENT_URL     | percent                           | url
 * PERCENT_FORM    | application/x-www-form-urlencoded | x-www-form-urlencoded, www-form-urlencoded, form-urlencoded, percent_plus url_plus, , percent-plus, url-plus, percentplus, urlplus
 * PERCENT_URLMIN  | percent_min                       | url_min
 * PERCENT_FORMMIN | form_min                          | form-urlencoded-min
 * PUNYCODE        | punycode                          | puny
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
	M_TEXTCODEC_CP037,           /*!< EBCDIC US Canada. */
	M_TEXTCODEC_CP500,           /*!< EBCDIC International. */
	M_TEXTCODEC_CP874,           /*!< Windows code page 874, Thai. */
	M_TEXTCODEC_CP1250,          /*!< Windows code page 1250, Central and Eastern Europe. */
	M_TEXTCODEC_CP1251,          /*!< Windows code page 1251, Bulgarian, Byelorussian, Macedonian, Russian, Serbian. */
	M_TEXTCODEC_CP1252,          /*!< Windows code page 1252, Western Europe. */
	M_TEXTCODEC_CP1253,          /*!< Windows code page 1253, Greek. */
	M_TEXTCODEC_CP1254,          /*!< Windows code page 1254, Turkish. */
	M_TEXTCODEC_CP1255,          /*!< Windows code page 1255, Hebrew. */
	M_TEXTCODEC_CP1256,          /*!< Windows code page 1256, Arabic. */
	M_TEXTCODEC_CP1257,          /*!< Windows code page 1257, Baltic languages. */
	M_TEXTCODEC_CP1258,          /*!< Windows code page 1258, Vietnamese. */
	M_TEXTCODEC_ISO8859_1,       /*!< ISO-8859-1. Latin 1, Western Europe. */
	M_TEXTCODEC_ISO8859_2,       /*!< ISO-8859-2. Latin 2, Central and Eastern Europe. */
	M_TEXTCODEC_ISO8859_3,       /*!< ISO-8859-3. Latin 3, Esperanto, Maltese. */
	M_TEXTCODEC_ISO8859_4,       /*!< ISO-8859-4. Latin 4, Baltic languages. */
	M_TEXTCODEC_ISO8859_5,       /*!< ISO-8859-5. Cyrillic. */
	M_TEXTCODEC_ISO8859_6,       /*!< ISO-8859-6. Arabic. */
	M_TEXTCODEC_ISO8859_7,       /*!< ISO-8859-7. Greek. */
	M_TEXTCODEC_ISO8859_8,       /*!< ISO-8859-8. Hebrew. */
	M_TEXTCODEC_ISO8859_9,       /*!< ISO-8859-9. Latin 5, Turkish. */
	M_TEXTCODEC_ISO8859_10,      /*!< ISO-8859-10. Latin 6, Nordic languages. */
	M_TEXTCODEC_ISO8859_11,      /*!< ISO-8859-11. Thai. */
	M_TEXTCODEC_ISO8859_13,      /*!< ISO-8859-13. Latin 7, Baltic languages. */
	M_TEXTCODEC_ISO8859_14,      /*!< ISO-8859-14. Latin 8, Celtic languages. */
	M_TEXTCODEC_ISO8859_15,      /*!< ISO-8859-15. Latin 9, Western Europe. */
	M_TEXTCODEC_ISO8859_16,      /*!< ISO-8859-16. Latin 10, South-Eastern Europe. */
	M_TEXTCODEC_PERCENT_URL,     /*!< Percent encoding for use as a URL rules. Must be utf-8. */
	M_TEXTCODEC_PERCENT_FORM,    /*!< Percent suitable for use as form data. Space as + and ~ encoded. Must be utf-8. */
	M_TEXTCODEC_PERCENT_URLMIN,  /*!< Minimal percent encoding. space and non ascii characters will be encoded but
	                                  all other reserved characters are not encoded. This is intended as a fix up
	                                  for URLs that have already been built. Typically built by hand. Must be utf-8. */
	M_TEXTCODEC_PERCENT_FORMMIN, /*!< Minimal percent encoding suitable for use as form data. Space as + and ~ encoded.
	                                  Space and non-ascii characters are encoded. All other reserved characters are not
	                                  encoded. This is intended as a fix up. Must be utf-8. */
	M_TEXTCODEC_PUNYCODE         /*!< IDNA Punycode (RFC 3492). Primarily used for DNS.
	                                  Error handlers will be ignore and all error conditions are failures. */
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
