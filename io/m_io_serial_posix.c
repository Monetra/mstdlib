/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_io_posix_common.h"
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#if defined (__APPLE__) && !defined(IOS)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/IOKitLib.h>
#  include <IOKit/serial/IOSerialKeys.h>
#  include <IOKit/IOBSD.h>
#endif

#include "m_io_serial_int.h"
/* XXX: currently needed for M_io_setnonblock() which should be moved */
#include "m_io_int.h"

struct M_io_handle {
    /* Settings */
    char                      path[1024];
    M_io_serial_flowcontrol_t flowcontrol;
    M_io_serial_mode_t        mode;
    M_io_serial_baud_t        baud;
    enum M_io_serial_flags    flags;

    /* State */
    int              fd;
    int              last_error_sys;
    struct termios   options;
    M_event_timer_t *disconnect_timer;
};

static void M_io_serial_close_handle(M_io_handle_t *handle)
{
    if (handle->disconnect_timer != NULL) {
        M_event_timer_remove(handle->disconnect_timer);
        handle->disconnect_timer = NULL;
    }

    if (handle->fd != -1) {
        if (!(handle->flags & M_IO_SERIAL_FLAG_NO_FLUSH_ON_CLOSE)) {
            /* Flush any pending data as otherwise on some systems close() may hang
             * forever if pending on a flow control operation */
            tcflush(handle->fd, TCIOFLUSH);
        }

        if (!(handle->flags & M_IO_SERIAL_FLAG_NO_RESTORE_ON_CLOSE)) {
            int rv;
            /* Restore serial port back to the original state before we opened it */
            rv = tcsetattr(handle->fd, TCSANOW, &handle->options);
            (void)rv; /* Appease coverity, we're closing the port, doesn't matter */
        }
        close(handle->fd);
    }
    handle->fd = -1;
}


static void M_io_serial_close(M_io_layer_t *layer)
{
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_event_t     *event  = M_io_get_event(io);
    M_io_handle_t *handle = M_io_layer_get_handle(layer);

    if (event && handle->fd != -1)
        M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->fd, M_EVENT_INVALID_SOCKET, 0, 0);

    M_io_serial_close_handle(handle);
}


static M_io_error_t M_io_serial_handle_set_defaults(M_io_handle_t *handle)
{
    struct termios options;

    if (handle == NULL || handle->fd == -1)
        return M_IO_ERROR_INVALID;

    if (tcgetattr(handle->fd, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    /* Copy so we can restore later */
    M_mem_copy(&handle->options, &options, sizeof(handle->options));

    /* NOTE: Should we just call cfmakeraw() instead? */
    options.c_cflag    &= ~((tcflag_t)(CSIZE|PARENB|PARODD|CSTOPB));
    options.c_cflag    |= (CS8 | CLOCAL | CREAD);  /* enable receiver and set local mode */
    options.c_lflag     = 0;
    options.c_oflag     = 0;
    options.c_iflag     = 0;
    /* We are using non-blocking I/O.  If we used '0' for VMIN, read() could return 0
     * and NOT mean any sort of disconnect occurred.  Setting VMIN=1 should make read
     * return -1 and errno be EWOULDBLOCK when no bytes are available instead. */
    options.c_cc[VMIN]  = 1;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(handle->fd, TCSANOW, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    return M_IO_ERROR_SUCCESS;
}


static M_bool M_io_serial_init_cb(M_io_layer_t *layer)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_event_t     *event  = M_io_get_event(io);
    M_io_error_t   err    = M_IO_ERROR_SUCCESS;

    if (handle->fd == -1) {

#ifndef O_NDELAY
#  define O_NDELAY O_NONBLOCK
#endif
        handle->fd = open(handle->path, O_RDWR | O_NDELAY | O_NOCTTY);
        if (handle->fd == -1) {
            handle->last_error_sys = errno;
            err = M_io_posix_err_to_ioerr(handle->last_error_sys);
            goto fail;
        }

        M_io_posix_fd_set_closeonexec(handle->fd, M_TRUE);

        if (!M_io_setnonblock(handle->fd)) {
            handle->last_error_sys = errno;
            err = M_io_posix_err_to_ioerr(handle->last_error_sys);
            goto fail;
        }

        err = M_io_serial_handle_set_defaults(handle);
        if (err != M_IO_ERROR_SUCCESS) {
            if (!(handle->flags & M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE) || err == M_IO_ERROR_NOTIMPL || err == M_IO_ERROR_INVALID) {
                goto fail;
            }
        }

        err = M_io_serial_handle_set_baud(handle, handle->baud);
        if (err != M_IO_ERROR_SUCCESS) {
            if (!(handle->flags & M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE) || err == M_IO_ERROR_NOTIMPL || err == M_IO_ERROR_INVALID) {
                goto fail;
            }
        }

        err = M_io_serial_handle_set_flowcontrol(handle, handle->flowcontrol);
        if (err != M_IO_ERROR_SUCCESS) {
            if (!(handle->flags & M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE) || err == M_IO_ERROR_NOTIMPL || err == M_IO_ERROR_INVALID) {
                goto fail;
            }
        }

        err = M_io_serial_handle_set_mode(handle, handle->mode);
        if (err != M_IO_ERROR_SUCCESS) {
            if (!(handle->flags & M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE) || err == M_IO_ERROR_NOTIMPL || err == M_IO_ERROR_INVALID) {
                goto fail;
            }
        }
    }

    /* Trigger connected soft event when registered with event handle */
    M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);

    /* Register fd to event subsystem */
    M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->fd, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, M_EVENT_CAPS_WRITE|M_EVENT_CAPS_READ);
    return M_TRUE;

fail:
    M_io_serial_close(layer);
    /* Trigger connected soft event when registered with event handle */
    M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, err);
    return M_TRUE; /* not usage issue */
}


static void M_io_serial_unregister_cb(M_io_layer_t *layer)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_event_t     *event  = M_io_get_event(io);

    M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->fd, M_EVENT_INVALID_SOCKET, 0, 0);
}


static M_io_error_t M_io_serial_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_io_error_t   err;

    if (layer == NULL || handle == NULL)
        return M_IO_ERROR_INVALID;

    err = M_io_posix_read(io, handle->fd, buf, read_len, &handle->last_error_sys, meta);
    if (M_io_error_is_critical(err))
        M_io_serial_close(layer);

    return err;
}


static M_io_error_t M_io_serial_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_io_error_t   err;
    if (layer == NULL || handle == NULL)
        return M_IO_ERROR_INVALID;

    err = M_io_posix_write(io, handle->fd, buf, write_len, &handle->last_error_sys, meta);
    if (M_io_error_is_critical(err))
        M_io_serial_close(layer);

    return err;
}

static void M_io_serial_destroy_handle(M_io_handle_t *handle)
{
    if (handle == NULL)
        return;

    M_io_serial_close_handle(handle);

    M_free(handle);
}

static void M_io_serial_destroy_cb(M_io_layer_t *layer)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);

    M_io_serial_destroy_handle(handle);
}


static M_io_state_t M_io_serial_state_cb(M_io_layer_t *layer)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    if (handle->fd == -1)
        return M_IO_STATE_ERROR;
    return M_IO_STATE_CONNECTED;
}


static M_bool M_io_serial_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);

    return M_io_posix_errormsg(handle->last_error_sys, error, err_len);
}


static void M_io_serial_disc_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *iodummy, void *arg)
{
    M_io_layer_t  *layer  = arg;
    M_io_handle_t *handle = M_io_layer_get_handle(layer);

    (void)event;
    (void)type;
    (void)iodummy;

    if (handle->fd != M_EVENT_INVALID_HANDLE) {
        M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
    }

    handle->disconnect_timer = NULL;
}


/* Add a 1/10th second delay for disconnect to ensure any data written has been flushed */
static M_bool M_io_serial_disconnect_cb(M_io_layer_t *layer)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);
    M_io_t        *io     = M_io_layer_get_io(layer);
    M_event_t     *event  = M_io_get_event(io);

    /* handle is already closed */
    if (handle->fd == M_EVENT_INVALID_HANDLE)
        return M_TRUE;

    /* Already disconnecting */
    if (handle->disconnect_timer)
        return M_FALSE;

    handle->disconnect_timer = M_event_timer_oneshot(event, 100 /* 1/10s */, M_TRUE, M_io_serial_disc_timer_cb, layer);

    return M_FALSE;
}


static const struct {
    M_io_serial_baud_t baud;
    speed_t            speed;
} M_io_serial_baud_conversion[] = {
    /* NOTE, must be listed in increasing order, and terminated with M_IO_SERIAL_BAUD_0 */
    { M_IO_SERIAL_BAUD_50,      B50      },
    { M_IO_SERIAL_BAUD_75,      B75      },
    { M_IO_SERIAL_BAUD_110,     B110     },
    { M_IO_SERIAL_BAUD_134,     B134     },
    { M_IO_SERIAL_BAUD_150,     B150     },
    { M_IO_SERIAL_BAUD_200,     B200     },
    { M_IO_SERIAL_BAUD_300,     B300     },
    { M_IO_SERIAL_BAUD_600,     B600     },
    { M_IO_SERIAL_BAUD_1200,    B1200    },
    { M_IO_SERIAL_BAUD_1800,    B1800    },
    { M_IO_SERIAL_BAUD_2400,    B2400    },
    { M_IO_SERIAL_BAUD_4800,    B4800    },
#ifdef B7200
    { M_IO_SERIAL_BAUD_7200,    B7200    },
#endif
    { M_IO_SERIAL_BAUD_9600,    B9600    },
#ifdef B14400
    { M_IO_SERIAL_BAUD_9600,    B14400   },
#endif
    { M_IO_SERIAL_BAUD_19200,   B19200   },
#ifdef B28800
    { M_IO_SERIAL_BAUD_19200,   B28800   },
#endif
    { M_IO_SERIAL_BAUD_38400,   B38400   },
#ifdef B57600
    { M_IO_SERIAL_BAUD_57600,   B57600   },
#endif
#ifdef B115200
    { M_IO_SERIAL_BAUD_115200,  B115200  },
#endif
#ifdef B230400
    { M_IO_SERIAL_BAUD_230400,  B230400  },
#endif
#ifdef B460800
    { M_IO_SERIAL_BAUD_460800,  B460800  },
#endif
#ifdef B500000
    { M_IO_SERIAL_BAUD_500000,  B500000  },
#endif
#ifdef B576000
    { M_IO_SERIAL_BAUD_576000,  B576000  },
#endif
#ifdef B921600
    { M_IO_SERIAL_BAUD_921600,  B921600  },
#endif
#ifdef B1000000
    { M_IO_SERIAL_BAUD_1000000, B1000000 },
#endif
#ifdef B1152000
    { M_IO_SERIAL_BAUD_1152000, B1152000 },
#endif
#ifdef B1500000
    { M_IO_SERIAL_BAUD_1500000, B1500000 },
#endif
#ifdef B2000000
    { M_IO_SERIAL_BAUD_2000000, B2000000 },
#endif
#ifdef B2500000
    { M_IO_SERIAL_BAUD_2500000, B2500000 },
#endif
#ifdef B3000000
    { M_IO_SERIAL_BAUD_3000000, B3000000 },
#endif
#ifdef B3500000
    { M_IO_SERIAL_BAUD_3500000, B3500000 },
#endif
#ifdef B4000000
    { M_IO_SERIAL_BAUD_4000000, B4000000 },
#endif
    { M_IO_SERIAL_BAUD_0,       B0       },
};


static speed_t M_io_serial_resolve_baud(M_io_serial_baud_t baud)
{
    size_t i;

    for (i=0; M_io_serial_baud_conversion[i].baud != M_IO_SERIAL_BAUD_0; i++) {
        /* Since our enum values are the same as the baud rate, this is safe.
         * Some OS's may not support all baud rates so we want to choose the
         * next highest */
        if (baud != M_IO_SERIAL_BAUD_0 && M_io_serial_baud_conversion[i].baud >= baud)
            return M_io_serial_baud_conversion[i].speed;
    }

    /* Requested BAUD_0, and we must be on it as that is the only reason the
     * loop terminates */
    if (M_io_serial_baud_conversion[i].baud == M_IO_SERIAL_BAUD_0)
        return M_io_serial_baud_conversion[i].speed;

    /* We must have requested a baud rate *greater* than supported.  Lets just return
     * the highest supported, which is the entry before BAUD_0 */
    return M_io_serial_baud_conversion[i-1].speed;
}


M_io_error_t M_io_serial_handle_set_baud(M_io_handle_t *handle, M_io_serial_baud_t baud)
{
    struct termios options;
    speed_t        speed;

    if (handle == NULL || handle->fd == -1)
        return M_IO_ERROR_INVALID;

    if (tcgetattr(handle->fd, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    speed = M_io_serial_resolve_baud(baud);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    if (tcsetattr(handle->fd, TCSANOW, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    handle->baud = baud;

    return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_serial_handle_set_flowcontrol(M_io_handle_t *handle, M_io_serial_flowcontrol_t flowcontrol)
{
    struct termios options;

    if (handle == NULL || handle->fd == -1)
        return M_IO_ERROR_INVALID;

    if (tcgetattr(handle->fd, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    /* Reset flow control-related bits */
#ifdef CRTSCTS
    options.c_cflag &= ~((tcflag_t)CRTSCTS);
#endif
    options.c_iflag &= ~((tcflag_t)(IXON | IXOFF));

    switch (flowcontrol) {
        case M_IO_SERIAL_FLOWCONTROL_NONE:
            break;
        case M_IO_SERIAL_FLOWCONTROL_HARDWARE:
#ifndef CRTSCTS
            return M_IO_ERROR_NOTIMPL;
#else
            options.c_cflag |= CRTSCTS;
            break;
#endif
        case M_IO_SERIAL_FLOWCONTROL_SOFTWARE:
            options.c_iflag |= (IXON | IXOFF);
            break;
    }

    if (tcsetattr(handle->fd, TCSANOW, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    handle->flowcontrol = flowcontrol;

    return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_serial_handle_set_mode(M_io_handle_t *handle, M_io_serial_mode_t mode)
{
    struct termios options;

    if (handle == NULL || handle->fd == -1)
        return M_IO_ERROR_INVALID;

    if (tcgetattr(handle->fd, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    /* Clear mode settings */
    options.c_cflag &= ~((tcflag_t)(CSIZE | PARENB | PARODD | CSTOPB));
#ifdef CMSPAR
    options.c_cflag &= ~((tcflag_t)CMSPAR);
#endif
    options.c_iflag &= ~((tcflag_t)(INPCK | ISTRIP));

    switch (mode & M_IO_SERIAL_MODE_MASK_BITS) {
        case M_IO_SERIAL_MODE_BITS_8:
            options.c_cflag |= CS8;
            break;
        case M_IO_SERIAL_MODE_BITS_7:
            options.c_cflag |= CS7;
            break;
        default:
            return M_IO_ERROR_INVALID;
    }

    switch (mode & M_IO_SERIAL_MODE_MASK_PARITY) {
        case M_IO_SERIAL_MODE_PARITY_NONE:
            break;
        case M_IO_SERIAL_MODE_PARITY_EVEN:
            options.c_cflag |= PARENB;
            options.c_iflag |= (INPCK | ISTRIP);
            break;
        case M_IO_SERIAL_MODE_PARITY_ODD:
            options.c_cflag |= (PARENB | PARODD);
            options.c_iflag |= (INPCK | ISTRIP);
            break;
#ifdef CMSPAR
        case M_IO_SERIAL_MODE_PARITY_SPACE:
            options.c_cflag |= (PARENB | CMSPAR);
            options.c_iflag |= (INPCK | ISTRIP);
            break;
        case M_IO_SERIAL_MODE_PARITY_MARK:
            options.c_cflag |= (PARENB | CMSPAR | PARODD);
            options.c_iflag |= (INPCK | ISTRIP);
            break;
#endif
        default:
            return M_IO_ERROR_NOTIMPL;
    }

    switch (mode & M_IO_SERIAL_MODE_MASK_STOPBITS) {
        case M_IO_SERIAL_MODE_STOPBITS_1:
            break;
        case M_IO_SERIAL_MODE_STOPBITS_2:
            options.c_cflag |= CSTOPB;
            break;
    }

    if (tcsetattr(handle->fd, TCSANOW, &options) != 0) {
        handle->last_error_sys = errno;
        return M_io_posix_err_to_ioerr(handle->last_error_sys);
    }

    handle->mode = mode;

    return M_IO_ERROR_SUCCESS;
}


M_io_serial_flowcontrol_t M_io_serial_handle_get_flowcontrol(M_io_handle_t *handle)
{
    return handle->flowcontrol;
}


M_io_serial_mode_t M_io_serial_handle_get_mode(M_io_handle_t *handle)
{
    return handle->mode;
}


M_io_serial_baud_t M_io_serial_handle_get_baud(M_io_handle_t *handle)
{
    return handle->baud;
}

static M_bool M_io_serial_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
    M_io_handle_t *handle = M_io_layer_get_handle(layer);

    (void)type;

    /* Pass on */
    return M_io_posix_process_cb(layer, handle->fd, handle->fd, type);
}

M_io_error_t M_io_serial_create(M_io_t **io_out, const char *path, M_io_serial_baud_t baud, M_io_serial_flowcontrol_t flowcontrol, M_io_serial_mode_t mode, M_uint32 flags)
{
    M_io_handle_t    *handle;
    M_io_callbacks_t *callbacks;

    if (io_out == NULL || M_str_isempty(path))
        return M_IO_ERROR_INVALID;

    handle              = M_malloc_zero(sizeof(*handle));
    handle->baud        = baud;
    handle->flowcontrol = flowcontrol;
    handle->mode        = mode;
    handle->flags       = flags;
    handle->fd          = -1;
    M_str_cpy(handle->path, sizeof(handle->path), path);

    /* Delay actual opening until attached to event handle so we can propagate a real OS error message */

    *io_out   = M_io_init(M_IO_TYPE_STREAM);
    callbacks = M_io_callbacks_create();
    M_io_callbacks_reg_init(callbacks, M_io_serial_init_cb);
    M_io_callbacks_reg_read(callbacks, M_io_serial_read_cb);
    M_io_callbacks_reg_write(callbacks, M_io_serial_write_cb);
    M_io_callbacks_reg_processevent(callbacks, M_io_serial_process_cb);
    M_io_callbacks_reg_unregister(callbacks, M_io_serial_unregister_cb);
    M_io_callbacks_reg_destroy(callbacks, M_io_serial_destroy_cb);
    M_io_callbacks_reg_state(callbacks, M_io_serial_state_cb);
    M_io_callbacks_reg_errormsg(callbacks, M_io_serial_errormsg_cb);
    M_io_callbacks_reg_disconnect(callbacks, M_io_serial_disconnect_cb);
    M_io_layer_add(*io_out, M_IO_SERIAL_NAME, handle, callbacks);
    M_io_callbacks_destroy(callbacks);

    return M_IO_ERROR_SUCCESS;
}

#if defined (__APPLE__) && !defined(IOS)

static kern_return_t FindSerialPorts(io_iterator_t *matchingServices)
{
    kern_return_t          kernResult;
    mach_port_t            masterPort;
    CFMutableDictionaryRef classesToMatch;

#if MAC_OS_X_VERSION_MIN_REQUIRED < 120000
    kernResult     = IOMasterPort(MACH_PORT_NULL, &masterPort);
#else
    kernResult     = IOMainPort(MACH_PORT_NULL, &masterPort);
#endif
    if (kernResult != KERN_SUCCESS)
        return kernResult;

    classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
    if (classesToMatch == NULL) {
    } else {
        CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));
    }
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, matchingServices);
    return kernResult;
}

M_io_serial_enum_t *M_io_serial_enum(M_bool include_modems)
{
    M_io_serial_enum_t *serenum = M_io_serial_enum_init();
    io_object_t         modemService;
    io_iterator_t       serialPortIterator;
    char                bsdPath[256];
    CFTypeRef           bsdPathAsCFString;
    Boolean             result;

    (void)include_modems;

    if (FindSerialPorts(&serialPortIterator) == KERN_SUCCESS) {
        while ((modemService = IOIteratorNext(serialPortIterator))) {
            bsdPathAsCFString = IORegistryEntryCreateCFProperty(modemService,
                                                                CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
            if (bsdPathAsCFString) {
                M_mem_set(bsdPath, 0, sizeof(bsdPath));
                result = CFStringGetCString(bsdPathAsCFString,
                                            bsdPath, sizeof(bsdPath), kCFStringEncodingASCII);
                CFRelease(bsdPathAsCFString);
                if (result) {
                    M_io_serial_enum_add(serenum, bsdPath, NULL);
                }
            }
        }
        IOObjectRelease(serialPortIterator);
    }


    return serenum;
}

#elif defined(IOS)

M_io_serial_enum_t *M_io_serial_enum(M_bool include_modems)
{
    (void)include_modems;
    /* Not supported */
    return M_io_serial_enum_init();
}

#else

static const char * const M_io_serial_paths[] = {
#if defined(__linux__)
    "/dev/ttyS*", "/dev/ttyUSB*", "/dev/ttyACM*",
#elif defined(__FreeBSD__)
    "/dev/cuaa*", "/dev/cuad*", "/dev/ucom*", "/dev/ttyU*",
#elif defined(__SCO_VERSION__) || defined(_SCO_ELF)
    "/dev/tty[1-9][A-Z]",
#elif defined(__sun__)
    "/dev/cua/*",
#endif
    NULL
};


M_io_serial_enum_t *M_io_serial_enum(M_bool include_modems)
{
    M_io_serial_enum_t *serenum = M_io_serial_enum_init();
    size_t              i;

    (void)include_modems;

    for (i=0; M_io_serial_paths[i] != NULL; i++) {
        char         *sdirname = M_fs_path_dirname(M_io_serial_paths[i], M_FS_SYSTEM_AUTO);
        char         *sbasename = M_fs_path_basename(M_io_serial_paths[i], M_FS_SYSTEM_AUTO);
        if (!M_str_isempty(sdirname) && !M_str_isempty(sbasename)) {
            size_t        j;
            M_list_str_t *matches = M_fs_dir_walk_strs(sdirname, sbasename, M_FS_DIR_WALK_FILTER_FILE);
            if (matches) {
                M_list_str_change_sorting(matches, M_LIST_STR_SORTASC);
                for (j=0; j<M_list_str_len(matches); j++) {
                    const char *devpath = M_list_str_at(matches, j);
                    if (devpath != NULL) {
                        char path[1024];
                        M_snprintf(path, sizeof(path), "%s/%s", sdirname, devpath);
                        M_io_serial_enum_add(serenum, path, NULL);
                    }
                }
                M_list_str_destroy(matches);
            }
        }
        M_free(sdirname);
        M_free(sbasename);
    }

    return serenum;
}
#endif


