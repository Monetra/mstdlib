/* The MIT License (MIT)
 * 
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

#ifndef __M_EMAIL_H__
#define __M_EMAIL_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_email Email
 *  \ingroup m_formats
 *
 * Email envelope reading and writing.
 *
 * This is a flexible implementation and does not auto encode or decode. Also, only
 * minimal data validation is performed. It is possible generate messages that are
 * not standard compliant but it should not be possible to generate a message that
 * with this module that cannot then be parsed by this module.
 *
 * Conforms to:
 * - RFC 5322 Internet Message Format
 *
 * Supported:
 * - RFC 6854 Update to Internet Message Format to Allow Group Syntax in the "From:" and "Sender:" Header Fields
 *
 * Not supported:
 * - RFC 2047 MIME (Multipurpose Internet Mail Extensions) Part Three: Message Header Extensions for Non-ASCII Text
 * - Splitting multipart within a multipart body part. The sub multipart will be returned as if it is all body data
 *
 * There are two types of message parsing supported.
 * - Stream based callback
 * - Simple reader (memory buffered)
 *
 * Currently supported Read:
 * - Callback
 * - Simple
 *
 * Currently support Write:
 * - Simple
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Error codes. */
typedef enum {
	M_EMAIL_ERROR_SUCCESS = 0,                /*!< Success. Data fully parsed data is present. More data is possible because email does not have a length indicator. However, a complete message has been seen. */
	M_EMAIL_ERROR_MOREDATA,                   /*!< Incomplete message, more data required. Not necessarily an error if parsing as data is streaming. */
	M_EMAIL_ERROR_STOP,                       /*!< Stop processing (Used by callback functions to indicate non-error but stop processing). */
	M_EMAIL_ERROR_INVALIDUSE,                 /*!< Invalid use. */
	M_EMAIL_ERROR_HEADER_INVALID,             /*!< Header is malformed.  */
	M_EMAIL_ERROR_ADDRESS,                    /*!< Address is malformed. */
	M_EMAIL_ERROR_MULTIPART_NOBOUNDARY,       /*!< Multipart message missing boundary. */
	M_EMAIL_ERROR_MULTIPART_HEADER_INVALID,   /*!< Multipart message missing boundary. */
	M_EMAIL_ERROR_MULTIPART_MISSING_DATA,     /*!< Multipart data missing. */
	M_EMAIL_ERROR_MULTIPART_INVALID,          /*!< Multipart is invalid. */
	M_EMAIL_ERROR_NOT_EMAIL,                  /*!< Not an EMAIL message. */
	M_EMAIL_ERROR_USER_FAILURE                /*!< Generic callback generated failure. */
} M_email_error_t;

/*! Email Content type. */
typedef enum {
	M_EMAIL_DATA_FORMAT_UNKNOWN = 0, /*! Could not determine the format of the data. */
	M_EMAIL_DATA_FORMAT_BODY,        /*!< Body. */
	M_EMAIL_DATA_FORMAT_MULTIPART    /*!< Data is multipart. */
} M_email_data_format_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_email_reader Email Stream Reader
 *  \ingroup m_email
 *
 * Stream reader used for parsing using callbacks.
 * Very useful for large Email messages.
 *
 * @{
 */

struct M_email_reader;
typedef struct M_email_reader M_email_reader_t;

/*! Function definition for reading headers.
 *
 * This will provide the full unparsed header.
 * This is always called for every header.
 * It may be called multiple times if a header appears multiple times.
 *
 * All headers will trigger this function including ones that have
 * their own dedicated callbacks. If headers are handled in their
 * resistive dedicated callback, they should be checked for and
 * ignored when this callback is called.
 *
 * A header appearing multiple times here means it was present multiple times.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_email_reader_to_func
 * \see M_email_reader_from_func
 * \see M_email_reader_cc_func
 * \see M_email_reader_bcc_func
 * \see M_email_reader_reply_to_func
 * \see M_email_reader_subject_func
 */
typedef M_email_error_t (*M_email_reader_header_func)(const char *key, const char *val, void *thunk);


/*! Function definition for To recipients
 *
 * This will be called for every address that appears as a To recipient.
 * A group with no listed recipients can also be received. If address only is desired
 * then address should be checked if empty before processing.
 *
 * Data combinations that could be passed as parameters:
 * - group, name, address
 * - name, address
 * - group, address
 * - group
 *
 * \param[in] group   Email group.
 * \param[in] name    Pretty name of recipient.
 * \param[in] address Email address of recipient.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_to_func)(const char *group, const char *name, const char *address, void *thunk);


/*! Function definition for From sender
 *
 * Data combinations that could be passed as parameters:
 * - group, name, address
 * - name, address
 * - group, address
 * - group
 *
 * \param[in] group   Email group.
 * \param[in] name    Pretty name of recipient.
 * \param[in] address Email address of recipient.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_from_func)(const char *group, const char *name, const char *address, void *thunk);


/*! Function definition for CC recipients 
 *
 * This will be called for every address that appears as a CC recipient.
 * A group with no listed recipients can also be received. If address only is desired
 * then address should be checked if empty before processing.
 *
 * Data combinations that could be passed as parameters:
 * - group, name, address
 * - name, address
 * - group, address
 * - group
 *
 * \param[in] group   Email group.
 * \param[in] name    Pretty name of recipient.
 * \param[in] address Email address of recipient.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_cc_func)(const char *group, const char *name, const char *address, void *thunk);


/*! Function definition for BCC recipients 
 *
 * This will be called for every address that appears as a BCC recipient.
 * A group with no listed recipients can also be received. If address only is desired
 * then address should be checked if empty before processing.
 *
 * Data combinations that could be passed as parameters:
 * - group, name, address
 * - name, address
 * - group, address
 * - group
 *
 * \param[in] group   Email group.
 * \param[in] name    Pretty name of recipient.
 * \param[in] address Email address of recipient.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_bcc_func)(const char *group, const char *name, const char *address, void *thunk);


/*! Function definition for Reply-To address
 *
 * Data combinations that could be passed as parameters:
 * - group, name, address
 * - name, address
 * - group, address
 * - group
 *
 * \param[in] group   Email group.
 * \param[in] name    Pretty name of recipient.
 * \param[in] address Email address of recipient.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_reply_to_func)(const char *group, const char *name, const char *address, void *thunk);


/*! Function definition for the message Subject
 *
 * \param[in] subject The subject.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_subject_func)(const char *subject, void *thunk);


/*! Function definition for header parsing completion.
 *
 * \param[in] format The format data was sent using.
 * \param[in] thunk  Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_header_done_func)(M_email_data_format_t format, void *thunk);


/*! Function definition for reading body data.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_body_func)(const unsigned char *data, size_t len, void *thunk);


/*! Function definition for reading multipart preamble.
 *
 * Typically the preamble should be ignored if present.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_preamble_func)(const unsigned char *data, size_t len, void *thunk);


/*! Function definition for completion of multipart preamble parsing.
 *
 * Only called if a preamble was present.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_preamble_done_func)(void *thunk);

/*! Function definition for reading multi part headers.
 *
 * This will provide the full unparsed header.
 * This is always called for every header.
 * It may be called multiple times if a header appears multiple times.
 * This is intended for informational use or if passing along data and
 * not altering any headers in the process.
 *
 * A header appearing multiple times here means it was present multiple times.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] idx   Part number the header belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_http_reader_header_func
 */
typedef M_email_error_t (*M_email_reader_multipart_header_func)(const char *key, const char *val, size_t idx, void *thunk);


/*! Function definition for completion of multipart part header parsing.
 *
 * \param[in] idx   Part number.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_header_done_func)(size_t idx, void *thunk);


/*! Function definition for multipart attachment meta data.
 *
 * Will only be called when a part is marked as an attachment.
 *
 * \param[in] content_type      The content type.
 * \param[in] transfer_encoding The format the data was received in. E.g. Base64, clear text, ext..
 * \param[in] filename          The filename of the attachment.
 * \param[in] idx               Part number.
 * \param[in] thunk             Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_header_attachment_func)(const char *content_type, const char *transfer_encoding, const char *filename, size_t idx, void *thunk);


/*! Function definition for reading multipart part data.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] idx   Partnumber the data belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_data_func)(const unsigned char *data, size_t len, size_t idx, void *thunk);


/*! Function definition for completion of multipart part data.
 *
 * \param[in] idx   Chunk number that has been completely processed.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_data_done_func)(size_t idx, void *thunk);


/*! Function definition for completion of parsing all multipart parts.
 *
 * Only called when data is chunked.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_data_finished_func)(void *thunk);


/*! Function definition for completion of multipart epilogue parsing.
 *
 * Only called if a epilogue was present.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_email_error_t (*M_email_reader_multipart_epilouge_func)(const unsigned char *data, size_t len, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Flags controlling reader behavior. */
typedef enum {
	M_EMAIL_READER_NONE = 0 /*!< Default operation. */
} M_email_reader_flags_t;


/*! Callbacks for various stages of parsing. */
struct M_email_reader_callbacks {
	M_email_reader_header_func                      header_func;
	M_email_reader_to_func                          to_func;
	M_email_reader_from_func                        from_func;
	M_email_reader_cc_func                          cc_func;
	M_email_reader_bcc_func                         bcc_func;
	M_email_reader_reply_to_func                    reply_to_func;
	M_email_reader_subject_func                     subject_func;
	M_email_reader_header_done_func                 header_done_func;
	M_email_reader_body_func                        body_func;
	M_email_reader_multipart_preamble_func          multipart_preamble_func;
	M_email_reader_multipart_preamble_done_func     multipart_preamble_done_func;
	M_email_reader_multipart_header_func            multipart_header_func;
	M_email_reader_multipart_header_done_func       multipart_header_done_func;
	M_email_reader_multipart_header_attachment_func multipart_header_attachment_func;
	M_email_reader_multipart_data_func              multipart_data_func;
	M_email_reader_multipart_data_done_func         multipart_data_done_func;
	M_email_reader_multipart_data_finished_func     multipart_data_finished_func;
	M_email_reader_multipart_epilouge_func          multipart_epilouge_func;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an email reader object.
 *
 * \param[in] cbs   Callbacks for processing.
 * \param[in] flags Flags controlling behavior.
 * \param[in] thunk Thunk passed to callbacks.
 *
 * \return Object.
 */
M_API M_email_reader_t *M_email_reader_create(struct M_email_reader_callbacks *cbs, M_uint32 flags, void *thunk);


/*! Destroy an email object.
 *
 * \param[in] emailr email reader object.
 */
M_API void M_email_reader_destroy(M_email_reader_t *emailr);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse email message from given data.
 *
 * When a parse returns without error but a full message has not been read, the
 * parse should be run again starting where the last parse stopped. The reader
 * can only be used once per complete message.
 *
 * Will _not_ return M_EMAIL_ERROR_MOREDATA. It is up to the caller to
 * determine when a full message has been read based on the callbacks that have
 * been called. The _done callbacks can indicate if all processing has
 * completed. If the message is not multipart it is impossible to determine if
 * a parse is complete.
 *
 * \param[in]  emailr   email reader object.
 * \param[in]  data     Data to parse.
 * \param[in]  data_len Length of data.
 * \param[out] len_read How much data was read.
 *
 * \return Result.
 */
M_API M_email_error_t M_email_reader_read(M_email_reader_t *emailr, const unsigned char *data, size_t data_len, size_t *len_read);

/*! @} */

/*! @} */

__END_DECLS

#endif /* __M_EMAIL_H__ */
