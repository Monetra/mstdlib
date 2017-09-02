/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

#ifndef __M_IO_BLOCK_H__
#define __M_IO_BLOCK_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_io_block Common Blocking (synchronous) IO functions
 *  \ingroup m_eventio
 *
 *  The io system can be used with blocking operations. An explicit event loop
 *  is not required. This allows the stacked layers to be utilized with a more
 *  traditional blocking design.
 *
 *  Here is an example of the system using the loopback io back end to simulate
 *  a network connection to a remote server.
 * 
 * \code{.c}
 *      #include <mstdlib/mstdlib.h>
 *      #include <mstdlib/mstdlib_io.h>
 *      
 *      int main(int argc, char *argv)
 *      {
 *          M_io_t     *io = NULL;
 *          M_buf_t    *buf;
 *          M_parser_t *parser;
 *          char       *out;
 *      
 *          buf    = M_buf_create();
 *          parser = M_parser_create(M_PARSER_FLAG_NONE);
 *          M_io_loopback_create(&io);
 *      
 *          M_io_block_connect(io);
 *      
 *          M_buf_add_str(buf, "TEST 123");
 *          M_io_block_write_from_buf(io, buf, M_TIMEOUT_INF);
 *      
 *          M_io_block_read_into_parser(io, parser, M_TIMEOUT_INF);
 *          out = M_parser_read_strdup(parser, M_parser_len(parser));
 *          M_printf("%s\n", out);
 *          M_free(out);
 *      
 *          M_buf_add_str(buf, "abc 456");
 *          M_io_block_write_from_buf(io, buf, M_TIMEOUT_INF);
 *      
 *          M_io_block_read_into_parser(io, parser, M_TIMEOUT_INF);
 *          out = M_parser_read_strdup(parser, M_parser_len(parser));
 *          M_printf("%s\n", out);
 *          M_free(out);
 *      
 *          M_parser_destroy(parser);
 *          M_buf_cancel(buf);
 *          M_io_block_disconnect(io);
 *          return 0;
 *      }
 * \endcode
 *
 * @{
 */


/*! Connect the io object to the remote end point.
 *
 * \param[in] io io object.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_connect(M_io_t *io);


/*! Accept an io connection.
 *
 * \param[out] io_out     io object created from the accept.
 * \param[in]  server_io  io object which was listening.
 * \param[in]  timeout_ms Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_accept(M_io_t **io_out, M_io_t *server_io, M_uint64 timeout_ms);


/*! Read from an io object.
 *
 * \param[in]  io         io object.
 * \param[out] buf        Buffer to store data read from io object.
 * \param[in]  buf_len    Lenght of provided buffer.
 * \param[out] len_read   Number of bytes fread from the io object.
 * \param[in]  timeout_ms Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_read(M_io_t *io, unsigned char *buf, size_t buf_len, size_t *len_read, M_uint64 timeout_ms);


/*! Read from an io object into an M_buf_t.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  io         io object.
 * \param[out] buf        Buffer to store data read from io object.
 * \param[in]  timeout_ms Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_read_into_buf(M_io_t *io, M_buf_t *buf, M_uint64 timeout_ms);


/*! Read from an io object into an M_parser_t.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  io         io object.
 * \param[out] parser     Parser to store data read from io object.
 * \param[in]  timeout_ms Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_read_into_parser(M_io_t *io, M_parser_t *parser, M_uint64 timeout_ms);


/*! Write data to an io object.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should try again.
 *
 * \param[in]  io          io object.
 * \param[in]  buf         Buffer to write from.
 * \param[in]  buf_len     Number of bytes in buffer to write.
 * \param[out] len_written Number of bytes from the buffer written.
 * \param[in]  timeout_ms  Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_write(M_io_t *io, const unsigned char *buf, size_t buf_len, size_t *len_written, M_uint64 timeout_ms);


/*! Write data to an io object from an M_buf_t.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should try again.
 *
 * \param[in] io         io object.
 * \param[in] buf        Buffer to write from.
 * \param[in] timeout_ms Amount of time in milliseconds to wait for data.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_write_from_buf(M_io_t *io, M_buf_t *buf, M_uint64 timeout_ms);

/*! Gracefully issue a disconnect to the communications object.
 *
 * \param[in] io io object.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_block_disconnect(M_io_t *io);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLOCK_H__ */
