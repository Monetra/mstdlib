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

#ifndef __M_HTTP_READER_H__
#define __M_HTTP_READER_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_http_reader HTTP Stream Reader
 *  \ingroup m_http
 *
 * @{
 */

struct M_http_reader;
typedef struct M_http_reader M_http_reader_t;

typedef M_http_error_t (*M_http_reader_start_func)(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk);

typedef M_http_error_t (*M_http_reader_header_func)(const char *key, const char *val, void *thunk);
typedef M_http_error_t (*M_http_reader_header_done_func)(void *thunk);

typedef M_http_error_t (*M_http_reader_body_func)(const unsigned char *data, size_t len, void *thunk);
typedef M_http_error_t (*M_http_reader_body_done_func)(void *thunk);

typedef M_http_error_t (*M_http_reader_chunk_extensions_func)(const char *key, const char *val, size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_chunk_extensions_done_func)(size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_chunk_data_func)(const unsigned char *data, size_t len, size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_chunk_data_done_func)(size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_chunk_data_finished_func)(void *thunk);

typedef M_http_error_t (*M_http_reader_multipart_preamble_func)(const unsigned char *data, size_t len, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_preamble_done_func)(void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_header_func)(const char *key, const char *val, size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_header_done_func)(size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_data_func)(const unsigned char *data, size_t len, size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_data_done_func)(size_t idx, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_epilouge_func)(const unsigned char *data, size_t len, void *thunk);
typedef M_http_error_t (*M_http_reader_multipart_epilouge_done_func)(void *thunk);

typedef M_http_error_t (*M_http_reader_trailer_func)(const char *key, const char *val, void *thunk);
typedef M_http_error_t (*M_http_reader_trailer_done_func)(void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_http_reader_callbacks {
	M_http_reader_start_func                   start_func;
	M_http_reader_header_func                  header_func;
	M_http_reader_header_done_func             header_done_func;
	M_http_reader_body_func                    body_func;
	M_http_reader_body_done_func               body_done_func;
	M_http_reader_chunk_extensions_func        chunk_extensions_func;
	M_http_reader_chunk_extensions_done_func   chunk_extensions_done_func;
	M_http_reader_chunk_data_func              chunk_data_func;
	M_http_reader_chunk_data_done_func         chunk_data_done_func;
	M_http_reader_chunk_data_finished_func     chunk_data_finished_func;
	M_http_reader_multipart_preamble_func      multipart_preamble_func;
	M_http_reader_multipart_preamble_done_func multipart_preamble_done_func;
	M_http_reader_multipart_header_func        multipart_header_func;
	M_http_reader_multipart_header_done_func   multipart_header_done_func;
	M_http_reader_multipart_data_func          multipart_data_func;
	M_http_reader_multipart_data_done_func     multipart_data_done_func;
	M_http_reader_multipart_epilouge_func      multipart_epilouge_func;
	M_http_reader_multipart_epilouge_done_func multipart_epilouge_done_func;
	M_http_reader_trailer_func                 trailer_func;
	M_http_reader_trailer_done_func            trailer_done_func;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an http reader object.
 *
 * \return Object.
 */
M_http_reader_t *M_http_reader_create(struct M_http_reader_callbacks *cbs, void *thunk);


/*! Destroy an http object.
 *
 * \param[in] httpr Http reader object.
 */
void M_http_reader_destroy(M_http_reader_t *httpr);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse http data.
 *
 * \param[in]  httpr    Http reader object.
 * \param[in]  data     Data to parser.
 * \param[in]  data_len Lenght of data.
 * \param[out] len_read How much data was read.
 *
 * \return Result.
 */
M_http_error_t M_http_reader_read(M_http_reader_t *httpr, const unsigned char *data, size_t data_len, size_t *len_read);

/*! @} */

__END_DECLS

#endif /* __M_HTTP_READER_H__ */

