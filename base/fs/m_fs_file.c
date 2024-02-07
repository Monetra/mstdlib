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
#include "fs/m_fs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_file_open(M_fs_file_t **fd, const char *path, size_t buf_size, M_uint32 mode, const M_fs_perms_t *perms)
{
    M_fs_error_t res;

    res = M_fs_file_open_sys(fd, path, mode, perms);
    if (res != M_FS_ERROR_SUCCESS)
        return res;

    (*fd)->buf_size    = buf_size;
    (*fd)->read_buf    = NULL;
    (*fd)->write_buf   = NULL;
    (*fd)->read_offset = 0;
    if ((*fd)->buf_size > 0) {
        (*fd)->read_buf    = M_buf_create();
        (*fd)->write_buf   = M_buf_create();
        (*fd)->read_offset = 0;
    }

    return M_FS_ERROR_SUCCESS;
}

void M_fs_file_close(M_fs_file_t *fd)
{
    if (fd == NULL)
        return;
    M_fs_file_sync(fd, M_FS_FILE_SYNC_BUFFER);
    M_buf_cancel(fd->read_buf);
    M_buf_cancel(fd->write_buf);
    M_fs_file_close_sys(fd);
}

M_fs_error_t M_fs_file_read(M_fs_file_t *fd, unsigned char *buf, size_t buf_len, size_t *read_len, M_uint32 flags)
{
    size_t         didread;
    size_t         read_total = 0;
    size_t         read_max;
    unsigned char *buf_block;
    M_fs_error_t   res;
    size_t         myread_len;

    if (fd == NULL || buf == NULL || buf_len == 0)
        return M_FS_ERROR_INVALID;

    if (read_len == NULL)
        read_len = &myread_len;
    *read_len = 0;

    read_max  = buf_len;
    buf_block = buf;

    /* Read data from the buffer. */
    if (fd->buf_size > 0) {
        /* flush the write buf because location in the file will have
         * moved and writes can't happen at the new location. This also
         * invalidates the read buffer since the read location may have changed. */
        if (M_buf_len(fd->write_buf) > 0)
            M_fs_file_sync(fd, M_FS_FILE_SYNC_BUFFER);

        /* If we can read from the user buffer do so. */
        if (M_buf_len(fd->read_buf) >= buf_len) {
            M_mem_copy(buf, M_buf_peek(fd->read_buf), buf_len);
            M_buf_drop(fd->read_buf, buf_len);
            *read_len = buf_len;
            /* Update the read offset for what was pulled out of the buffer. */
            fd->read_offset -= (M_int64)buf_len;
            return M_FS_ERROR_SUCCESS;
        }

        /* Otherwise we'll fill the user buffer with the buffer size plus the request size minus the amount
         * already in the buffer. This will have the buffer temporarily be buf_len (max) more than the requested
         * buffer size but buf_len will be dumped from the read buffer into the request buffer. This will try
         * to keep the read buffer maxed. */
        read_max  = fd->buf_size + buf_len - M_buf_len(fd->read_buf);
        buf_block = M_buf_direct_write_start(fd->read_buf, &read_max);
    }

    /* Read data from disk. */
    do {
        res = M_fs_file_read_sys(fd, buf_block+read_total, read_max-read_total, &didread);
        read_total += didread;
    } while (flags & M_FS_FILE_RW_FULLBUF && res == M_FS_ERROR_SUCCESS && didread != 0 && read_total < read_max);

    *read_len = read_total;

    /* Read from the read buffer into the request buffer any additional data. */
    if (fd->buf_size > 0 && read_total > 0) {
        M_buf_direct_write_end(fd->read_buf, read_total);

        /* Fill the request buffer. */
        read_max = M_MIN(M_buf_len(fd->read_buf), buf_len);
        M_mem_copy(buf, M_buf_peek(fd->read_buf), read_max);
        M_buf_drop(fd->read_buf, read_max);
        /* Offset is now how much read vs how much written to request buffer. */
        fd->read_offset += (M_int64)(read_total - read_max);
        *read_len        = read_max;
    }

    return res;
}

M_fs_error_t M_fs_file_read_bytes(const char *path, size_t max_read, unsigned char **buf, size_t *bytes_read)
{
    M_fs_error_t   res;
    M_fs_file_t   *fd         = NULL;
    M_buf_t       *buf_int;
    size_t         didread    = 0;

    if (path == NULL || buf == NULL)
        return M_FS_ERROR_INVALID;

    if (bytes_read)
        *bytes_read = 0;

    res = M_fs_file_open(&fd, path, 0, M_FS_FILE_MODE_READ|M_FS_FILE_MODE_NOCREATE, NULL);
    if (res != M_FS_ERROR_SUCCESS) {
        return res;
    }

    buf_int = M_buf_create();

    do {
        size_t         temp_size = 1024;
        unsigned char *temp      = M_buf_direct_write_start(buf_int, &temp_size);
        didread = 0;
        res     = M_fs_file_read(fd, temp, temp_size, &didread, M_FS_FILE_RW_NORMAL);
        M_buf_direct_write_end(buf_int, didread);

        if (max_read != 0 && M_buf_len(buf_int) > max_read) {
            res = M_FS_ERROR_FILE_2BIG;
        }
    } while (res == M_FS_ERROR_SUCCESS && didread != 0);
    M_fs_file_close(fd);

    if (res != M_FS_ERROR_SUCCESS) {
        M_buf_cancel(buf_int);
        return res;
    }

    *buf = M_buf_finish(buf_int, bytes_read);
    return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_write(M_fs_file_t *fd, const unsigned char *buf, size_t count, size_t *wrote_len, M_uint32 flags)
{
    size_t       left        = 0;
    size_t       wrote       = 0;
    size_t       offset      = 0;
    size_t       mywrote_len;
    M_fs_error_t res         = M_FS_ERROR_SUCCESS;

    if (fd == NULL || buf == NULL || count == 0)
        return M_FS_ERROR_INVALID;

    if (wrote_len == NULL)
        wrote_len = &mywrote_len;
    *wrote_len = 0;

    if (fd->buf_size > 0) {
        /* If a write comes in a move to the correct file offset in case it's different due to buffered reading.
         * Use the system call because "seek" will kill the read and write bufs. */
        M_fs_file_seek_sys(fd, fd->read_offset * -1, M_FS_FILE_SEEK_CUR);
        /* Kill the read buf and set the offset correctly. */
        M_buf_truncate(fd->read_buf, 0);
        fd->read_offset = 0;

        /* Add the data to the write buf. */
        M_buf_add_bytes(fd->write_buf, buf, count);
        *wrote_len = count;

        /* Only write when the buffer is full. */
        if (M_buf_len(fd->write_buf) < fd->buf_size) {
            return M_FS_ERROR_SUCCESS;
        }

        left = M_buf_len(fd->write_buf);
        buf  = (const unsigned char *)M_buf_peek(fd->write_buf);
    } else {
        left = count;
    }

    do {
        res     = M_fs_file_write_sys(fd, buf+offset, left, &wrote);
        left   -= wrote;
        offset += wrote;
    } while (flags & M_FS_FILE_RW_FULLBUF && res == M_FS_ERROR_SUCCESS && left > 0);

    if (fd->buf_size > 0) {
        M_buf_drop(fd->write_buf, offset);
    } else {
        *wrote_len = offset;
    }
    return res;
}

M_fs_error_t M_fs_file_write_bytes(const char *path, const unsigned char *buf, size_t write_len, M_uint32 mode, size_t *bytes_written)
{
    M_fs_file_t      *fd     = NULL;
    M_fs_error_t      res    = M_FS_ERROR_SUCCESS;
    M_fs_file_mode_t  mymode = M_FS_FILE_MODE_WRITE;
    size_t            left   = 0;
    size_t            wrote  = 0;

    if (path == NULL || buf == NULL)
        return M_FS_ERROR_INVALID;

    if (bytes_written)
        *bytes_written = 0;

    if (mode & M_FS_FILE_MODE_APPEND) {
        mymode |= M_FS_FILE_MODE_APPEND;
    } else {
        mymode |= M_FS_FILE_MODE_OVERWRITE;
    }

    res = M_fs_file_open(&fd, path, 0, mymode, NULL);
    if (res != M_FS_ERROR_SUCCESS) {
        return res;
    }

    left = (write_len > 0) ? write_len : M_str_len((const char *)buf);
    res  = M_fs_file_write(fd, buf, left, &wrote, M_FS_FILE_RW_FULLBUF);
    if (bytes_written)
        *bytes_written = wrote;

    M_fs_file_close(fd);
    return res;
}

M_fs_error_t M_fs_file_seek(M_fs_file_t *fd, M_int64 offset, M_fs_file_seek_t from)
{
    if (fd == NULL)
        return M_FS_ERROR_INVALID;

    if (offset == 0 && from == M_FS_FILE_SEEK_CUR)
        return M_FS_ERROR_SUCCESS;

    /* flush the write buf because location in the file will have
     * moved and writes can't happen at the new location. This will
     * kill the write buf. */
    if (M_buf_len(fd->write_buf) > 0)
        M_fs_file_sync(fd, M_FS_FILE_SYNC_BUFFER);

    /* If we're seeking ahead from the current position, and the destination is still inside
     * our read buffer, just drop bytes from the read buffer and update the read offset.
     */
    if (from == M_FS_FILE_SEEK_CUR && offset > 0 && M_buf_len(fd->read_buf) >= (M_uint64)offset) {
        M_buf_drop(fd->read_buf, (size_t)offset);
        /* Update the read offset to reflect what was skipped in the buffer. */
        fd->read_offset -= offset;
        return M_FS_ERROR_SUCCESS;
    }

    /* Otherwise, move the file offset to the expected location and kill the read buf. */
    M_buf_truncate(fd->read_buf, 0);
    if (fd->read_offset != 0 && from == M_FS_FILE_SEEK_CUR)
        offset -= fd->read_offset;
    fd->read_offset = 0;

    /* Seek to the requested location. */
    return M_fs_file_seek_sys(fd, offset, from);
}

M_fs_error_t M_fs_file_sync(M_fs_file_t *fd, M_uint32 type)
{
    unsigned char *data;
    size_t         len;
    size_t         buf_size;
    size_t         wrote_len = 0;
    M_fs_error_t   res       = M_FS_ERROR_SUCCESS;

    /* We only care about syncing the write buffer if there is something to sync. */
    if (type & M_FS_FILE_SYNC_BUFFER && M_buf_len(fd->write_buf) > 0) {
        len = M_buf_len(fd->write_buf);
        /* Remove the size and buffer so we can trick write into thinking it's
         * unbuffered and write all data to the file even if there is less than
         * the buffer size. */
        buf_size        = fd->buf_size;
        fd->buf_size    = 0;
        data            = M_buf_finish(fd->write_buf, &len);

        /* Note: There is no need to touch the read buffer because it's impossible
         * for the read buffer to have data when the write buffer also has data. A write
         * will  invalidate the read buffer and a read will invalidate the write buffer. */

        res = M_fs_file_write(fd, data, len, &wrote_len, M_FS_FILE_RW_FULLBUF);

        /* Re enable buffering. */
        fd->buf_size  = buf_size;
        fd->write_buf = M_buf_create();

        if (res != M_FS_ERROR_SUCCESS || len != wrote_len) {
            /* Add any data that couldn't be written back to the write buffer
             * and error. */
            M_buf_add_bytes(fd->write_buf, data+wrote_len, len-wrote_len);
            res = M_FS_ERROR_IO;
        }
        M_free(data);
    }

    if (res == M_FS_ERROR_SUCCESS && type & M_FS_FILE_SYNC_OS)
        res = M_fs_file_fsync_sys(fd);

    return res;
}
