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

#ifdef HAVE_UNISTD_H
#  include <unistd.h> /* write */
#endif

#ifdef HAVE_IO_H
#  include <io.h> /* _write */
#  define write(a,b,c) _write(a,b,(unsigned int)(c))
#endif

#include <mstdlib/mstdlib.h>
#include <float.h> /* Needed for DBL_EPSILON */
#include "m_parser_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define M_STR_FMT_WRITE_BUF_LEN 512

typedef enum {
	M_STR_FMT_ENDPOINT_STREAM = 0,
	M_STR_FMT_ENDPOINT_MFD,
	M_STR_FMT_ENDPOINT_FD,
	M_STR_FMT_ENDPOINT_SBUF,
	M_STR_FMT_ENDPOINT_MBUF
} M_str_fmt_endpoint_t;

typedef enum {
	M_STR_FMT_DATA_TYPE_INT = 0,
	M_STR_FMT_DATA_TYPE_SORT,
	M_STR_FMT_DATA_TYPE_CHAR,
	M_STR_FMT_DATA_TYPE_LONG,
	M_STR_FMT_DATA_TYPE_LONGLONG,
	M_STR_FMT_DATA_TYPE_DOUBLE,
	M_STR_FMT_DATA_TYPE_SIZET,
	M_STR_FMT_DATA_TYPE_VOIDP
} M_str_fmt_data_type_t;

typedef enum {
	M_STR_FMT_SIGN_TYPE_NEG = 0,
	M_STR_FMT_SIGN_TYPE_NEGPOS,
	M_STR_FMT_SIGN_TYPE_POSSPACE
} M_str_fmt_sign_type_t;

typedef struct {
	M_str_fmt_endpoint_t endpoint;
	union {
		FILE        *stream;
		M_fs_file_t *mfd;
		M_buf_t     *mbuf;

		struct {
			int      fd;
			char     write_buf[M_STR_FMT_WRITE_BUF_LEN];
			size_t   len;
		} file;

		struct {
			char    *buf;
			size_t   pos;
			size_t   len;
		} sbuf;
	} o;
} M_str_fmt_t;

typedef struct {
	va_list ap;
} M_str_fmt_varargs_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_str_fmt_reverse_bytes(char *bytes, size_t len)
{
	char   b;
	size_t i;

	if (len == 0)
		return;

	i = len/2;
	len--;
	for ( ; i-->0; ) {
		b            = bytes[i];
		bytes[i]     = bytes[len-i];
		bytes[len-i] = b;
	}
}

static size_t M_str_fmt_integer_to_str(M_uint64 val, unsigned short base, M_bool uppercase, char *bytes, size_t len)
{
	const char *base_chars;
	size_t      i = 0;

	if (base > 16 || bytes == NULL || len == 0)
		return 0;

	M_mem_set(bytes, 0, len);

	/* Base conversion characters. */
	if (uppercase) {
		base_chars = "0123456789ABCDEF";
	} else {
		base_chars = "0123456789abcdef";
	}

	/* Convert integer to string. */
	do {
		bytes[i]  = base_chars[val % base];
		val      /= base;
		i++;
	} while (val > 0 && i < len-1);

	/* Numbers are reversed. Put them in the right order. */
	M_str_fmt_reverse_bytes(bytes, i);

	return i;
}

static M_bool M_float_equals(double a, double b)
{
	if (M_ABS(a - b) <= DBL_EPSILON * M_ABS(a))
		return M_TRUE;

	return M_FALSE;
}

/* Only converts positive numbers without fractional part. */
static size_t M_str_fmt_double_to_str(double val, char *bytes, size_t len)
{
	double temp;
	size_t i = 0;

	if (bytes == NULL || len == 0)
		return 0;

	do {
		/* Move 1 digit to after the decimal sign */
		temp = val * 0.1;
		/* Split the whole and fractional parts. */
		M_math_modf(temp, &val);
		bytes[i] = "0123456789"[(size_t)((temp - val) * 10.0) % 10];
		i++;
	} while (!M_float_equals(val, 0.0) && i < len-1);

	M_str_fmt_reverse_bytes(bytes, i);

	return i;
}

static double M_str_fmt_fpow(double b, unsigned int p)
{
	double r = 1;
	while (p > 0) {
		r *= b;
		p--;
	}
	return r;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_str_fmt_flush_buffers(M_str_fmt_t *data)
{
	size_t len;

	switch (data->endpoint) {
		case M_STR_FMT_ENDPOINT_FD:
			len = data->o.file.len;
			if (len == 0) {
				break;
			}
			if (write(data->o.file.fd, data->o.file.write_buf, len) != (ssize_t)len) {
				return M_FALSE;
			}
			break;
		case M_STR_FMT_ENDPOINT_SBUF:
			if (data->o.sbuf.buf == NULL || data->o.sbuf.len == 0) {
				break;
			}
			data->o.sbuf.buf[data->o.sbuf.pos] = '\0';
			break;
		case M_STR_FMT_ENDPOINT_STREAM:
		case M_STR_FMT_ENDPOINT_MBUF:
		case M_STR_FMT_ENDPOINT_MFD:
			break;
	}

	return M_TRUE;
}

static ssize_t M_str_fmt_add_bytes(M_str_fmt_t *data, const unsigned char *b, size_t len)
{
	size_t tlen;
	size_t wrote_len;

	if (len <= 0)
		return 0;

	tlen = len;

	switch (data->endpoint) {
		case M_STR_FMT_ENDPOINT_STREAM:
			if (fwrite(b, 1, tlen, data->o.stream) != tlen) {
				return -1;
			}
			break;
		case M_STR_FMT_ENDPOINT_MFD:
			/* Using fullbuf flag to have it write everything. We'll fail if everything couldn't be written. */
			if (M_fs_file_write(data->o.mfd, b, len, &wrote_len, M_FS_FILE_RW_FULLBUF) != M_FS_ERROR_SUCCESS || wrote_len != len) {
				return -1;
			}
			break;
		case M_STR_FMT_ENDPOINT_FD:
			if (data->o.file.len+tlen >= M_STR_FMT_WRITE_BUF_LEN) {
				if (write(data->o.file.fd, data->o.file.write_buf, data->o.file.len) != (ssize_t)data->o.file.len) {
					return -1;
				}
				data->o.file.len = 0;
				while (tlen > M_STR_FMT_WRITE_BUF_LEN) {
					if (write(data->o.file.fd, b, M_STR_FMT_WRITE_BUF_LEN) != M_STR_FMT_WRITE_BUF_LEN) {
						return -1;
					}
					b    += M_STR_FMT_WRITE_BUF_LEN;
					tlen -= M_STR_FMT_WRITE_BUF_LEN;
				}
			}
			if (tlen > 0) {
				M_mem_copy(data->o.file.write_buf+data->o.file.len, b, tlen);
				data->o.file.len += tlen;
			}
			break;
		case M_STR_FMT_ENDPOINT_SBUF:
			if (data->o.sbuf.buf == NULL || data->o.sbuf.len == 0) {
				break;
			}
			/* -1 to leave room for the NULL terminator. */
			if (tlen > data->o.sbuf.len - data->o.sbuf.pos - 1) {
				tlen = data->o.sbuf.len - data->o.sbuf.pos - 1;
			}

			if (tlen != 0) {
				M_mem_copy(data->o.sbuf.buf+data->o.sbuf.pos, b, tlen);
				data->o.sbuf.pos += tlen;
			}
			break;
		case M_STR_FMT_ENDPOINT_MBUF:
			if (data->o.mbuf == NULL) {
				break;
			}
			M_buf_add_bytes(data->o.mbuf, b, tlen);
			break;
	}

	return (ssize_t)len;
}

static ssize_t M_str_fmt_add_byte(M_str_fmt_t *data, unsigned char b)
{
	return M_str_fmt_add_bytes(data, &b, 1);
}

static ssize_t M_str_fmt_add_fill(M_str_fmt_t *data, unsigned char b, size_t len)
{
	size_t i;

	for (i=0; i<len; i++) {
		if (M_str_fmt_add_byte(data, b) != 1) {
			return -1;
		}
	}

	return (ssize_t)len;
}

static ssize_t M_str_fmt_add_bytes_just(M_str_fmt_t *data, const unsigned char *bytes, size_t len, unsigned char pad_char, size_t pad_len, M_bool ljust)
{
	if (bytes == NULL) {
		bytes = (const unsigned char *)"<NULL>";
		len   = M_str_len((const char *)bytes);
	}

	/* Determine how many characters will be used for padding. */
	if (len >= pad_len) {
		pad_len = 0;
	} else {
		pad_len -= len;
	}

	if (pad_len > 0 && !ljust) {
		if (M_str_fmt_add_fill(data, pad_char, pad_len) != (ssize_t)pad_len) {
			return -1;
		}
	}

	if (M_str_fmt_add_bytes(data, bytes, len) != (ssize_t)len)
		return -1;

	if (pad_len > 0 && ljust) {
		if (M_str_fmt_add_fill(data, pad_char, pad_len) != (ssize_t)pad_len) {
			return -1;
		}
	}

	return (ssize_t)(len+pad_len);
}

/* Will either write add the sign character to the output or will add it to a byte array based on the pad character.
 * Can update pad_len. The added data shouldn't be counted toward padding but if we need to add it to
 * the data we need to increase the pad_len by the amount added. */
static ssize_t M_str_fmt_add_sign(M_str_fmt_t *data, unsigned char *bytes, size_t data_len, M_bool pos, M_str_fmt_sign_type_t sign_type, unsigned char pad_char, size_t *pad_len)
{
	unsigned char sign = '-';

	if (pos && sign_type == M_STR_FMT_SIGN_TYPE_NEG)
		return 0;

	switch (sign_type) {
		case M_STR_FMT_SIGN_TYPE_NEG:
			break;
		case M_STR_FMT_SIGN_TYPE_NEGPOS:
			if (pos) {
				sign = '+';
			}
			break;
		case M_STR_FMT_SIGN_TYPE_POSSPACE:
			if (pos) {
				sign = ' ';
			}
			break;
	}

	if (pad_char == '0') {
		*pad_len -= M_MIN(*pad_len, 1);
		return M_str_fmt_add_byte(data, sign);
	}

	M_mem_move(bytes+1, bytes, data_len+1);
	bytes[0] = sign;
	return 0;
}

/* Will either write add the prefix to the output or will add it to a byte array based on the pad character.
 * Can update pad_len. The added data shouldn't be counted toward padding but if we need to add it to
 * the data we need to increase the pad_len by the amount added. */
static ssize_t M_str_fmt_add_prefix(M_str_fmt_t *data, unsigned char *bytes, size_t data_len, unsigned short base, M_bool uppercase, unsigned char pad_char, size_t *pad_len)
{
	const char *prefix = NULL;
	size_t      len;

	switch (base) {
		case 8:
			if (bytes[0] != '0') {
				prefix = "0";
			}
			break;
		case 16:
			if (uppercase) {
				prefix = "0X";
			} else {
				prefix = "0x";
			}
			break;
	}

	if (M_str_isempty(prefix))
		return 0;

	len = M_str_len(prefix);
	if (pad_char == '0') {
		*pad_len -= M_MIN(*pad_len, len);
		return M_str_fmt_add_bytes_just(data, (const unsigned char *)prefix, len, 0, 0, M_FALSE);
	}

	M_mem_move(bytes+len, bytes, data_len+1);
	M_mem_copy(bytes, prefix, len);
	return 0;
}

static ssize_t M_str_fmt_add_integer_just(M_str_fmt_t *data, M_uint64 val, unsigned short base, M_bool pos, M_str_fmt_sign_type_t sign_type, M_bool add_prefix, M_bool uppercase, unsigned char pad_char, size_t pad_len, M_bool ljust)
{
	/* Max characters for unsigned 64 bit int is 19.
 	 * We have 1 unsigned 64 bit integers = 20.
	 * We can have a sign characters = 21.
	 * We can have a prefix (max 2 characters) = 23.
	 * 32 byte buffer will hold the largest number
	 * possible. */
	unsigned char bytes[32] = { 0 };
	ssize_t       olen      = 0;

	if (base == 0 || base > 16)
		return -1;

	M_str_fmt_integer_to_str(val, base, uppercase, (char *)bytes, sizeof(bytes));
	/* Add the sign character. */
	olen += M_str_fmt_add_sign(data, bytes, M_str_len((const char *)bytes), pos, sign_type, pad_char, &pad_len);
	/* Add a prefix. */
	if (add_prefix) {
		olen += M_str_fmt_add_prefix(data, bytes, M_str_len((const char *)bytes), base, uppercase, pad_char, &pad_len);
	}

	olen += M_str_fmt_add_bytes_just(data, bytes, M_str_len((const char *)bytes), pad_char, pad_len, ljust);
	return olen;
}

static ssize_t M_str_fmt_add_double_just(M_str_fmt_t *data, double dval, M_str_fmt_sign_type_t sign_type, size_t prec_len, unsigned char pad_char, size_t pad_len, M_bool ljust)
{
	/* Max double value is 309 characters.
 	 * We need to hold 2 more for '-/+' and '.'.
	 * We need another 100 (our limit) for the fractional amount.
	 * 411 min.
	 */
	unsigned char bytes[512] = { 0 };
	double        int_part  = 0;
	double        frac_part = 0;
	double        p10;
	M_bool        pos       = M_TRUE;
	ssize_t       olen      = 0;
	size_t        offset    = 0;
	size_t        added;

	/* Limit to 100 decimal digits. */
	if (prec_len > 100)
		prec_len = 100;

	if (dval < 0.0) {
		dval = M_ABS(dval);
		pos   = M_FALSE;
	}

	/* Check if this is a number that can be represented.
 	 *
	 * nan: x != x
	 * inf: x == x && x-x != 0.0
 	 *
	 * is a valid and portable way to check but it will cause a warning when
	 * "-Wfloat-equal" is set. Don't try to silence warnings for these
	 * checks! */
#if defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
	if (dval != dval || (dval == dval && dval - dval != 0.0)) {
		const char *const_temp = "inf";
		if (dval != dval) {
			const_temp = "nan";
		}
#if defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#  pragma GCC diagnostic pop
#endif
		M_mem_copy(bytes, const_temp, M_str_len(const_temp));
		olen += M_str_fmt_add_sign(data, bytes, M_str_len((const char *)bytes), pos, sign_type, pad_char, &pad_len);
		olen += M_str_fmt_add_bytes_just(data, bytes, M_str_len((const char *)bytes), pad_char, pad_len, ljust);
		return olen;
	}

	/* Split integer and frac parts.
 	 *
	 * This will limit to a max integer of M_uint64.
	 */
	frac_part = M_math_modf(dval, &int_part);

	/* Convert the frac part into a whole number. */
	p10       = M_str_fmt_fpow(10, (unsigned int)prec_len);
	frac_part = M_math_round(frac_part * p10);
	/* Ensure frac_part doesn't have a fractional part if there
 	 * where more digits than our precision. */
	M_math_modf(frac_part, &frac_part);

	/* Handle rounding. */
	while (frac_part >= p10) {
		int_part++;
		frac_part -= p10;
	}

	/* Convert the integer part to a string. */
	offset = M_str_fmt_double_to_str(int_part, (char *)bytes, sizeof(bytes));

	/* Convert the fractional part to a string. */
	if (prec_len > 0) {
		bytes[offset] = '.';
		offset++;

		/* Determine the number of fractional digits. */
		added = M_str_fmt_double_to_str(frac_part, (char *)bytes+offset, sizeof(bytes)-offset);

		/* If less digits were added then there are total for
 		 * the fractional part we need to prepend with zeros. We had something
		 * like 0.001. When converted to a whole number the leading 0's
		 * would be stripped. We need to put them back. */
		if (added < prec_len) {
			/* Fill in any leading 0's that were stripped due to the integer conversion. */
			M_mem_move(bytes+offset+prec_len-added, bytes+offset, added);
			M_mem_set(bytes+offset, '0', prec_len-added);
			offset += prec_len-added;
		}
		offset += added;
	}

	if (offset < sizeof(bytes)) {
		olen += M_str_fmt_add_sign(data, bytes, offset, pos, sign_type, pad_char, &pad_len);
	}
	olen += M_str_fmt_add_bytes_just(data, bytes, M_str_len((const char *)bytes), pad_char, pad_len, ljust);
	return olen;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* NOTE: short and char are promoted to int when pass though '...' We're going to cast
 * the value to the approbate type so it won't exceed what's expected. */
static M_int64 M_str_fmt_get_signed_integer(M_str_fmt_data_type_t data_type, M_str_fmt_varargs_t *ap)
{
	M_int64 val = 0;

	switch (data_type) {
		case M_STR_FMT_DATA_TYPE_INT:
			val = va_arg(ap->ap, int);
			break;
		case M_STR_FMT_DATA_TYPE_SORT:
			val = (short)va_arg(ap->ap, int);
			break;
		case M_STR_FMT_DATA_TYPE_CHAR:
			val = (char)va_arg(ap->ap, int);
			break;
		case M_STR_FMT_DATA_TYPE_LONG:
			val = va_arg(ap->ap, long);
			break;
		case M_STR_FMT_DATA_TYPE_LONGLONG:
			val = va_arg(ap->ap, M_int64);
			break;
		case M_STR_FMT_DATA_TYPE_SIZET:
			val = va_arg(ap->ap, ssize_t);
			break;
		case M_STR_FMT_DATA_TYPE_VOIDP:
			val = (M_int64)(M_intptr)va_arg(ap->ap, void *);
			break;
		case M_STR_FMT_DATA_TYPE_DOUBLE:
			/* Not an integer. */
			break;
	}

	return val;
}

static M_uint64 M_str_fmt_get_unsigned_integer(M_str_fmt_data_type_t data_type, M_str_fmt_varargs_t *ap)
{
	M_uint64 val = 0;

	switch (data_type) {
		case M_STR_FMT_DATA_TYPE_INT:
			val = va_arg(ap->ap, unsigned int);
			break;
		case M_STR_FMT_DATA_TYPE_SORT:
			val = (unsigned short)va_arg(ap->ap, unsigned int);
			break;
		case M_STR_FMT_DATA_TYPE_CHAR:
			val = (unsigned char)va_arg(ap->ap, unsigned int);
			break;
		case M_STR_FMT_DATA_TYPE_LONG:
			val = va_arg(ap->ap, unsigned long);
			break;
		case M_STR_FMT_DATA_TYPE_LONGLONG:
			val = va_arg(ap->ap, M_uint64);
			break;
		case M_STR_FMT_DATA_TYPE_SIZET:
			val = va_arg(ap->ap, size_t);
			break;
		case M_STR_FMT_DATA_TYPE_VOIDP:
			val = (M_uint64)(M_uintptr)va_arg(ap->ap, void *);
			break;
		case M_STR_FMT_DATA_TYPE_DOUBLE:
			/* Not an integer. */
			break;
	}

	return val;
}

static double M_str_fmt_get_double(M_str_fmt_data_type_t data_type, M_str_fmt_varargs_t *ap)
{
	double val = 0;

	switch (data_type) {
		case M_STR_FMT_DATA_TYPE_DOUBLE:
			val = va_arg(ap->ap, double);
			break;
		case M_STR_FMT_DATA_TYPE_INT:
		case M_STR_FMT_DATA_TYPE_SORT:
		case M_STR_FMT_DATA_TYPE_CHAR:
		case M_STR_FMT_DATA_TYPE_LONG:
		case M_STR_FMT_DATA_TYPE_LONGLONG:
		case M_STR_FMT_DATA_TYPE_SIZET:
		case M_STR_FMT_DATA_TYPE_VOIDP:
			/* Not floating point. */
			break;
	}

	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_str_fmt_handle_control_check_byte(M_parser_t *parser, unsigned char c)
{
	unsigned char b;

	if (!M_parser_peek_byte(parser, &b))
		return M_FALSE;

	if (b == c) {
		M_parser_consume(parser, 1);
		return M_TRUE;
	}

	return M_FALSE;
}

static ssize_t M_str_fmt_handle_control(M_str_fmt_t *data, M_parser_t *parser, M_str_fmt_varargs_t *ap)
{
	const char            *const_temp;
	unsigned char          b;
	M_bool                 add_prefix = M_FALSE;
	M_bool                 ljust      = M_FALSE;
	M_str_fmt_sign_type_t  sign_type  = M_STR_FMT_SIGN_TYPE_NEG;
	M_bool                 have_len   = M_FALSE;
	M_bool                 ret;
	unsigned char          pad_char   = ' ';
	size_t                 pad_len    = 0;
	size_t                 input_len  = 0;
	/* Outlen values:
 	 * -2 = end of stream with open control.
	 * -1 = fatal error propagated down.
	 *  >= 0 = Output length. */
	ssize_t               out_len    = -2;
	size_t                start_len  = 0;
	M_str_fmt_data_type_t data_type  = M_STR_FMT_DATA_TYPE_INT;
	unsigned char         temp[4];
	M_int64               sdval;
	M_uint64              udval;
	double                ddval;

	start_len = M_parser_len(parser);
	if (start_len == 0)
		goto parse_error;

	if (!M_parser_peek_byte(parser, &b))
		goto parse_error;

	/* Check for escaped %. */
	if (b == '%') {
		/* Read one, wrote 1. Returning length from
		 * function so it has the opportunity to error.
		 * That said, it will always return 1. */
		M_parser_consume(parser, 1);
		return M_str_fmt_add_byte(data, b);
	}

	/* Check the modifiers. */
	while (M_parser_peek_byte(parser, &b) && (b == '-' || b == '+' || b == '#' || b == '0' || b == ' ')) {
		ret = M_str_fmt_handle_control_check_byte(parser, '-');
		if (ret) {
			pad_char = ' ';
			ljust    = M_TRUE;
		}
		/* Check sign (add + sign) for numerics. */
		ret = M_str_fmt_handle_control_check_byte(parser, '+');
		if (ret) {
			sign_type = M_STR_FMT_SIGN_TYPE_NEGPOS;
		}
		ret = M_str_fmt_handle_control_check_byte(parser, ' ');
		if (ret && sign_type != M_STR_FMT_SIGN_TYPE_NEGPOS) {
			sign_type = M_STR_FMT_SIGN_TYPE_POSSPACE;
		}
		/* Check prefix. */
		ret = M_str_fmt_handle_control_check_byte(parser, '#');
		if (ret) {
			add_prefix = M_TRUE;
		}
		/* Check padding character. */
		ret = M_str_fmt_handle_control_check_byte(parser, '0');
		if (ret && !ljust) {
			pad_char = '0';
		}
	}

	while (M_parser_read_byte(parser, &b)) {
		switch (b) {
			/* Type of length. Output '.' Input
 			 *
			 * Notes:
			 *   - Output is the total length of the output including anything
			 *     added like sign or prefix. If this is larger than the input
			 *     length padding will be added. If this is smaller than the
			 *     input length, this will be ignored and the output length
			 *     will be the input length. This will not cause truncation.
			 *   - Input Applies to strings and floating point only.
			 *     - Strings: How many characters to read of the argument.
			 *       Truncation happens on the right no matter if it's left or
			 *       right justified.
			 *     - Floating point: This is the number of decimal digits.
			 */
			case '.':
				if (have_len)
					goto parse_error;
				have_len = M_TRUE;
				break;

			/* Length is given. */
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (have_len) {
					input_len = input_len * 10 + (size_t)(b - '0');
				} else {
					pad_len = pad_len * 10 + (size_t)(b - '0');
				}
				break;

			/* Length is an argument. */
			case '*':
				sdval = va_arg(ap->ap, int);
				if (have_len) {
					if (sdval > 0) {
						input_len = (size_t)sdval;
					}
				} else {
					if (sdval > 0) {
						pad_len = (size_t)sdval;
					}
				}
				break;

			/* Type modifiers. */
			case 'h':
				if (data_type == M_STR_FMT_DATA_TYPE_INT) {
					data_type = M_STR_FMT_DATA_TYPE_SORT;
				} else if (data_type == M_STR_FMT_DATA_TYPE_SORT) {
					data_type = M_STR_FMT_DATA_TYPE_CHAR;
				} else {
					goto parse_error;
				}
				break;
			case 'l':
				if (data_type == M_STR_FMT_DATA_TYPE_INT) {
					data_type = M_STR_FMT_DATA_TYPE_LONG;
				} else if (data_type == M_STR_FMT_DATA_TYPE_LONG) {
					data_type = M_STR_FMT_DATA_TYPE_LONGLONG;
				} else {
					goto parse_error;
				}
				break;
			case 'I':
				/* 'I' is a Windows specific modifier we're supporting. */
				if (M_parser_peek_bytes(parser, 2, temp)) {
					if (data_type != M_STR_FMT_DATA_TYPE_INT) {
						goto parse_error;
					}
					if (M_mem_eq(temp, "64", 2)) {
						data_type = M_STR_FMT_DATA_TYPE_LONGLONG;
						M_parser_consume(parser, 2);
					} else if (M_mem_eq(temp, "32", 2)) {
						M_parser_consume(parser, 2);
					}
					break;
				}
				/* 'I' without 64 or 32 after is
				 * the same as 'z'
				 */
				/* Falls through. */
			case 'z':
				if (data_type != M_STR_FMT_DATA_TYPE_INT) {
					goto parse_error;
				}
				data_type = M_STR_FMT_DATA_TYPE_SIZET;
				break;

			/* Everything after this point is not a modifier. These
 			 * will end the control character parse. */

			/* Integral types.
 			 *
			 * Notes:
			 *   - Input len does not apply to these.
			 */
			case 'd':
			case 'i':
				sdval   = M_str_fmt_get_signed_integer(data_type, ap);
				out_len = M_str_fmt_add_integer_just(data, (M_uint64)M_ABS(sdval), 10, sdval<0?M_FALSE:M_TRUE, sign_type, M_FALSE, M_FALSE, pad_char, pad_len, ljust);
				goto done;

			case 'o':
			case 'O':
				udval   = M_str_fmt_get_unsigned_integer(data_type, ap);
				out_len = M_str_fmt_add_integer_just(data, udval, 8, M_TRUE, M_STR_FMT_SIGN_TYPE_NEG, add_prefix, b=='O'?M_TRUE:M_FALSE, pad_char, pad_len, ljust);
				goto done;
			case 'u':
				udval   = M_str_fmt_get_unsigned_integer(data_type, ap);
				out_len = M_str_fmt_add_integer_just(data, udval, 10, M_TRUE, M_STR_FMT_SIGN_TYPE_NEG, M_FALSE, M_FALSE, pad_char, pad_len, ljust);
				goto done;
			case 'x':
			case 'X':
				udval   = M_str_fmt_get_unsigned_integer(data_type, ap);
				out_len = M_str_fmt_add_integer_just(data, udval, 16, M_TRUE, M_STR_FMT_SIGN_TYPE_NEG, add_prefix, b=='X'?M_TRUE:M_FALSE, pad_char, pad_len, ljust);
				goto done;

			/* Pseudo integral type.
 			 *
			 * Notes:
			 *   - Equivalent to "%#x", "%#lx", "%#llx".
			 */
			case 'p':
			case 'P':
				udval   = M_str_fmt_get_unsigned_integer(M_STR_FMT_DATA_TYPE_VOIDP, ap);
				out_len = M_str_fmt_add_integer_just(data, udval, 16, M_TRUE, M_STR_FMT_SIGN_TYPE_NEG, M_TRUE, b=='P'?M_TRUE:M_FALSE, pad_char, pad_len, ljust);
				goto done;

			/* Floating point types. */
			case 'e':
			case 'E':
			case 'f':
			case 'F':
			case 'g':
			case 'G':
				data_type = M_STR_FMT_DATA_TYPE_DOUBLE;
				ddval     = M_str_fmt_get_double(data_type, ap);
				if (!have_len) {
					/* Default is 6 decimal digits. */
					input_len = 6;
				}
				out_len = M_str_fmt_add_double_just(data, ddval, sign_type, input_len, pad_char, pad_len, ljust);
				goto done;

			/* Character. */
			case 'c':
				b       = (unsigned char)va_arg(ap->ap, int);
				out_len = M_str_fmt_add_bytes_just(data, &b, 1, pad_char, pad_len, ljust);
				goto done;

			/* String. */
			case 's':
				const_temp = va_arg(ap->ap, char *);
				if (have_len) {
					input_len = M_str_len_max(const_temp, input_len);
				} else {
					input_len = M_str_len(const_temp);
				}
				out_len = M_str_fmt_add_bytes_just(data, (const unsigned char *)const_temp, input_len, ' ', pad_len, ljust);
				goto done;

			default:
parse_error:
				out_len = M_str_fmt_add_byte(data, '%');
				if (out_len == -1) {
					goto done;
				}
				out_len += M_str_fmt_add_fill(data, '?', start_len-M_parser_len(parser));
				goto done;
		}
	}

done:

	if (out_len == -2)
		goto parse_error;

	if (out_len == -1)
		return -1;

	/* Add how many bytes were read to the argument byte size. */
	return out_len;
}

static ssize_t M_str_fmt_do_print(M_str_fmt_t *data, const char *fmt, va_list vap)
{
	M_parser_t          parser;
	ssize_t             outlen = 0;
	ssize_t             t;
	unsigned char       b;
	M_str_fmt_varargs_t ap;

	M_parser_init(&parser, (const unsigned char *)fmt, M_str_len(fmt), M_PARSER_FLAG_NONE);

	/* We put the va_list into a struct so we can pass the struct around and be
 	 * referenced.
	 *
	 * va_list can be implemented in different ways and one some systems we
	 * found we cannot pass them by reference. Even though the standard implies
	 * we should be able to.
	 *
	 * Further, we cannot pass the va_list directly because it's position will
	 * reset when passed to a function. Every function will start at position 0
	 * every time it's called.
	 *
	 * We need to copy the va_list into one held by the struct because, again,
	 * some systems won't allow us to pass by reference. So the struct can't
	 * hold a pointer to the va_list. va_copy was added in C99 so we can't
	 * assume it's always available. __va_copy is the draft name of the function.
	 *
	 * On systems that don't have either, we try other methods to copy based
	 * on the underlying type.
	 */
#if defined(HAVE_VA_COPY)
	va_copy(ap.ap, vap);
#elif defined(HAVE___VA_COPY)
	__va_copy(ap.ap, vap);
#elif defined(VA_LIST_IS_ARRAY_TYPE)
	/* a = b */
	M_mem_copy(&(ap.ap), &vap, sizeof(vap));
#elif defined(VA_LIST_IS_POINTER_TYPE)
	/* *a = *b */
	M_mem_copy(ap.ap, vap, sizeof(*vap));
#else
#  error va_copy not possible
#endif

	while (M_parser_len(&parser) > 0) {
		if (!M_parser_read_byte(&parser, &b)) {
			outlen = -1;
			goto done;
		}

		if (b == '%') {
			t = M_str_fmt_handle_control(data, &parser, &ap);
		} else {
			t = M_str_fmt_add_byte(data, b);
		}

		if (t == -1) {
			outlen = -1;
			goto done;
		}

		outlen += t;
	}

	if (!M_str_fmt_flush_buffers(data))
		outlen = -1;

done:
#if defined(HAVE_VA_COPY) || defined(HAVE___VA_COPY)
	va_end(ap.ap);
#endif

	return outlen;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * FILE
 */

ssize_t M_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	M_str_fmt_t data;

	M_mem_set(&data, 0, sizeof(data));

	if (stream == NULL)
		return -1;

	data.endpoint = M_STR_FMT_ENDPOINT_STREAM;
	data.o.stream = stream;

	return M_str_fmt_do_print(&data, fmt, ap);
}

ssize_t M_fprintf(FILE *stream, const char *fmt, ...)
{
	ssize_t len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vfprintf(stream, fmt, ap);
	va_end(ap);

	return len;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * mstdlib file desc
 */

ssize_t M_vmdprintf(M_fs_file_t *fd, const char *fmt, va_list ap)
{
	M_str_fmt_t data;

	M_mem_set(&data, 0, sizeof(data));

	if (fd == NULL)
		return -1;

	data.endpoint = M_STR_FMT_ENDPOINT_MFD;
	data.o.mfd    = fd;

	return M_str_fmt_do_print(&data, fmt, ap);
}

ssize_t M_mdprintf(M_fs_file_t *fd, const char *fmt, ...)
{
	ssize_t len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vmdprintf(fd, fmt, ap);
	va_end(ap);

	return len;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * os int file desc
 */

ssize_t M_vdprintf(int fd, const char *fmt, va_list ap)
{
	M_str_fmt_t data;

	M_mem_set(&data, 0, sizeof(data));

	if (fd <= 0)
		return -1;

	data.endpoint   = M_STR_FMT_ENDPOINT_FD;
	data.o.file.fd  = fd;
	data.o.file.len = 0;

	return M_str_fmt_do_print(&data, fmt, ap);
}

ssize_t M_dprintf(int fd, const char *fmt, ...)
{
	ssize_t len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vdprintf(fd, fmt, ap);
	va_end(ap);

	return len;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * stdout
 */

ssize_t M_vprintf(const char *fmt, va_list ap)
{
	return M_vfprintf(stdout, fmt, ap);
}

ssize_t M_printf(const char *fmt, ...)
{
	ssize_t len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vprintf(fmt, ap);
	va_end(ap);

	return len;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* string
*/

/* fill */
size_t M_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	M_str_fmt_t data;
	ssize_t     rv;

	data.endpoint   = M_STR_FMT_ENDPOINT_SBUF;
	/* str could be NULL but we handle that when writing the output. Calling
 	 * with NULL will still return the calculated length. */
	data.o.sbuf.buf = str;
	data.o.sbuf.pos = 0;
	data.o.sbuf.len = size;

	rv = M_str_fmt_do_print(&data, fmt, ap);
	if (rv < 0)
		return 0;
	return (size_t)rv;
}

size_t M_snprintf(char *str, size_t size, const char *fmt, ...)
{
	size_t  len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vsnprintf(str, size, fmt, ap);
	va_end(ap);

	return len;
}


/* allocate */
size_t M_vasprintf(char **ret, const char *fmt, va_list ap)
{
	M_buf_t *buf;
	size_t   len;

	buf = M_buf_create();
	M_vbprintf(buf, fmt, ap);

	if (ret != NULL) {
		*ret = M_buf_finish_str(buf, &len);
	} else {
		len = M_buf_len(buf);
		M_buf_cancel(buf);
	}
	return len;
}

size_t M_asprintf(char **ret, const char *fmt, ...)
{
	size_t  len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vasprintf(ret, fmt, ap);
	va_end(ap);

	return len;
}


/* M_buf */
size_t M_vbprintf(M_buf_t *buf, const char *fmt, va_list ap)
{
	M_str_fmt_t data;
	ssize_t     rv;

	data.endpoint = M_STR_FMT_ENDPOINT_MBUF;
	data.o.mbuf   = buf;

	rv = M_str_fmt_do_print(&data, fmt, ap);
	if (rv < 0)
		return 0;
	return (size_t)rv;
}

size_t M_bprintf(M_buf_t *buf, const char *fmt, ...)
{
	size_t  len;
	va_list ap;

	va_start(ap, fmt);
	len = M_vbprintf(buf, fmt, ap);
	va_end(ap);

	return len;
}
