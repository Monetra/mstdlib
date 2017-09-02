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

#ifndef __M_IO_SERIAL_H__
#define __M_IO_SERIAL_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_serial Serial Port I/O functions
 *  \ingroup m_eventio_base
 * 
 * Serial Port I/O functions
 *
 * @{
 */

/*! Baud rate. */
enum M_io_serial_baud {
	M_IO_SERIAL_BAUD_0       = 0,   /*!< Used to terminate the connection (drop DTR) */
	M_IO_SERIAL_BAUD_50      = 50,
	M_IO_SERIAL_BAUD_75      = 75,
	M_IO_SERIAL_BAUD_110     = 110,
	M_IO_SERIAL_BAUD_134     = 134,
	M_IO_SERIAL_BAUD_150     = 150,
	M_IO_SERIAL_BAUD_200     = 200,
	M_IO_SERIAL_BAUD_300     = 300,
	M_IO_SERIAL_BAUD_600     = 600,
	M_IO_SERIAL_BAUD_1200    = 1200,
	M_IO_SERIAL_BAUD_1800    = 1800,
	M_IO_SERIAL_BAUD_2400    = 2400,
	M_IO_SERIAL_BAUD_4800    = 4800,
	M_IO_SERIAL_BAUD_7200    = 7200,  /* Not POSIX */
	M_IO_SERIAL_BAUD_9600    = 9600,
	M_IO_SERIAL_BAUD_14400   = 14400, /* Not POSIX */
	M_IO_SERIAL_BAUD_19200   = 19200,
	M_IO_SERIAL_BAUD_28800   = 28800, /* Not POSIX */
	M_IO_SERIAL_BAUD_38400   = 38400,
	/* Bauds below are not technically POSIX.1 and may not exist on all systems */
	M_IO_SERIAL_BAUD_57600   = 57600,
	M_IO_SERIAL_BAUD_115200  = 115200,
	M_IO_SERIAL_BAUD_128000  = 128000,
	M_IO_SERIAL_BAUD_230400  = 230400,
	M_IO_SERIAL_BAUD_256000  = 256000,
	M_IO_SERIAL_BAUD_460800  = 460800,
	M_IO_SERIAL_BAUD_500000  = 500000,
	M_IO_SERIAL_BAUD_576000  = 576000,
	M_IO_SERIAL_BAUD_921600  = 921600,
	M_IO_SERIAL_BAUD_1000000 = 1000000,
	M_IO_SERIAL_BAUD_1152000 = 1152000,
	M_IO_SERIAL_BAUD_1500000 = 1500000,
	M_IO_SERIAL_BAUD_2000000 = 2000000,
	M_IO_SERIAL_BAUD_2500000 = 2500000,
	M_IO_SERIAL_BAUD_3000000 = 3000000,
	M_IO_SERIAL_BAUD_3500000 = 3500000,
	M_IO_SERIAL_BAUD_4000000 = 4000000
};
typedef enum M_io_serial_baud M_io_serial_baud_t;


/*! Types of flow control. */
enum M_io_serial_flowcontrol {
	M_IO_SERIAL_FLOWCONTROL_NONE     = 0,
	M_IO_SERIAL_FLOWCONTROL_HARDWARE = 1,
	M_IO_SERIAL_FLOWCONTROL_SOFTWARE = 2
};
typedef enum M_io_serial_flowcontrol M_io_serial_flowcontrol_t;

#define M_IO_SERIAL_MODE_MASK_BITS     0x000F
#define M_IO_SERIAL_MODE_MASK_PARITY   0x00F0
#define M_IO_SERIAL_MODE_MASK_STOPBITS 0x0F00
#define M_IO_SERIAL_MODE_BITS_8        0x0000 /* CS8 */
#define M_IO_SERIAL_MODE_BITS_7        0x0001 /* CS7 */
#define M_IO_SERIAL_MODE_PARITY_NONE   0x0000 /* &= ~(PARENB | PARODD | CMSPAR) */
#define M_IO_SERIAL_MODE_PARITY_EVEN   0x0010 /* PARENB */
#define M_IO_SERIAL_MODE_PARITY_ODD    0x0020 /* PARENB | PARODD */
#define M_IO_SERIAL_MODE_PARITY_MARK   0x0030 /* PARENB | CMSPAR | PARODD -- CMSPAR may be undefined */
#define M_IO_SERIAL_MODE_PARITY_SPACE  0x0040 /* PARENB | CMSPAR - &= ~PARODD -- CMSPAR may be undefined */
#define M_IO_SERIAL_MODE_STOPBITS_1    0x0000 /* &= ~(CSTOPB) */
#define M_IO_SERIAL_MODE_STOPBITS_2    0x0100 /* CSTOPB */

/*! Mode. */
enum M_io_serial_mode {
	/* Mode is split up into 3 4-bit sections */
	M_IO_SERIAL_MODE_8N1 = M_IO_SERIAL_MODE_BITS_8 | M_IO_SERIAL_MODE_PARITY_NONE  | M_IO_SERIAL_MODE_STOPBITS_1,
	M_IO_SERIAL_MODE_7E1 = M_IO_SERIAL_MODE_BITS_7 | M_IO_SERIAL_MODE_PARITY_EVEN  | M_IO_SERIAL_MODE_STOPBITS_1,
	M_IO_SERIAL_MODE_7O1 = M_IO_SERIAL_MODE_BITS_7 | M_IO_SERIAL_MODE_PARITY_ODD   | M_IO_SERIAL_MODE_STOPBITS_1,
};
typedef enum M_io_serial_mode M_io_serial_mode_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Flags to control behavior.
 *
 * These flags provide work around for broken system.
 */
enum M_io_serial_flags {
	M_IO_SERIAL_FLAG_NONE                   = 0,      /*!< Default, assume strict and proper behavior                */
	M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE = 1 << 0, /*!< Ignore any termios (baud, mode, flow) setting failures.   *
	                                                   *   Some serial port emulators may intentionally fail.        */
	M_IO_SERIAL_FLAG_NO_FLUSH_ON_CLOSE      = 1 << 1, /*!< Do not flush any pending data on close.  This may confuse *
	                                                   *   or lock up some serial port emulators.                    */
	M_IO_SERIAL_FLAG_NO_RESTORE_ON_CLOSE    = 1 << 2, /*!< Do not restore termios (baud, mode, flow) settings on     *
	                                                   *   close.  It is a best practice but often does not provide  *
	                                                   *   any real benefit.                                         */
	M_IO_SERIAL_FLAG_ASYNC_TIMEOUT          = 1 << 3, /*!< Windows Only. For Asynchronous reads use a timeout value  *
	                                                   *   rather than infinite as some drivers may not allow        *
	                                                   *   canceling of async reads (such as Citrix serial           *
	                                                   *   forwarding). Not used if BUSY_POLLING is used             */
	M_IO_SERIAL_FLAG_BUSY_POLLING           = 1 << 4  /*!< Windows Only. Perform busy-polling in a separate thread   *
	                                                   *   rather than using asynchronous reads.  This may work      *
	                                                   *   around driver issues that do not properly support         *
	                                                   *   Overlapped IO.                                            */
};
typedef enum M_io_serial_flags M_io_serial_flags_t;

struct M_io_serial_enum;
typedef struct M_io_serial_enum M_io_serial_enum_t;


/*! Create a serial connection.
 *
 * \param[out] io_out      io object for communication.
 * \param[in]  path        Path to serial device.
 * \param[in]  baud        Baud rate.
 * \param[in]  flowcontrol Flow control method.
 * \param[in]  mode        Mode.
 * \param[in]  flags       M_io_serial_flags_t mapping. M_IO_SERIAL_FLAG_IGNORE_TERMIOS_FAILURE may need to
 *                         be enabled for some "virtual" serial ports, but the device will still open and be usable.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_create(M_io_t **io_out, const char *path, M_io_serial_baud_t baud, M_io_serial_flowcontrol_t flowcontrol, M_io_serial_mode_t mode, M_uint32 flags /* enum M_io_serial_flags */);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set the baud rate on a serial io object.
 *
 * \param[in] io   io object.
 * \param[in] baud Baud rate.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_set_baud(M_io_t *io, M_io_serial_baud_t baud);


/*! Set the flow control on a serial io object.
 *
 * \param[in] io          io object.
 * \param[in] flowcontrol Flow control method.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_set_flowcontrol(M_io_t *io, M_io_serial_flowcontrol_t flowcontrol);


/*! Set the mode on a serial io object.
 *
 * \param[in] io   io object.
 * \param[in] mode Mode.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_set_mode(M_io_t *io, M_io_serial_mode_t mode);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the flow control of an serial io object.
 *
 * \param[in]  io          io object.
 * \param[out] flowcontrol Flow control method to return.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_get_flowcontrol(M_io_t *io, M_io_serial_flowcontrol_t *flowcontrol);


/*! Get the mode of an serial io object.
 *
 * \param[in]  io   io object.
 * \param[out] mode Mode to return.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_get_mode(M_io_t *io, M_io_serial_mode_t *mode);


/*! Get the baud rate of an serial io object.
 *
 * \param[in]  io   io object.
 * \param[out] baud Baud to return.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_serial_get_baud(M_io_t *io, M_io_serial_baud_t *baud);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a serial enumeration object.
 *
 * Use to determine what serial devices are connected. On some OS's this may
 * be a list of device ports and not not necessarily what's connected.
 *
 * \return Serial enumeration object.
 */
M_API M_io_serial_enum_t *M_io_serial_enum(M_bool include_modems);


/*! Destroy a serial enumeration object.
 *
 * \param[in] serenum Serial enumeration object.
 */
M_API void M_io_serial_enum_destroy(M_io_serial_enum_t *serenum);


/*! Number of serial objects in the enumeration.
 * 
 * \param[in] serenum Serial enumeration object.
 *
 * \return Count of serial devices.
 */
M_API size_t M_io_serial_enum_count(const M_io_serial_enum_t *serenum);


/*! Path of serial device as reported by the device.
 *
 * \param[in] serenum Serial enumeration object.
 * \param[in] idx     Index in serial enumeration.
 *
 * \return String.
 */
M_API const char *M_io_serial_enum_path(const M_io_serial_enum_t *serenum, size_t idx);

/*! Name of serial device.
 *
 * \param[in] serenum Serial enumeration object.
 * \param[in] idx     Index in serial enumeration.
 *
 * \return String.
 */
M_API const char *M_io_serial_enum_name(const M_io_serial_enum_t *serenum, size_t idx);

/*! @} */

__END_DECLS

#endif /* __M_IO_SERIAL_H__ */
