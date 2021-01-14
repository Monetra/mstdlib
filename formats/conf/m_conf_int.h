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

#ifndef __M_CONF_INT_H__
#define __M_CONF_INT_H__


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_text.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Registration types. */
typedef enum M_conf_reg_type_t {
	M_CONF_REG_TYPE_NONE   = 0,
	M_CONF_REG_TYPE_BUF    = 1 << 0,
	M_CONF_REG_TYPE_STRDUP = 1 << 1,
	M_CONF_REG_TYPE_INT8   = 1 << 2,
	M_CONF_REG_TYPE_INT16  = 1 << 3,
	M_CONF_REG_TYPE_INT32  = 1 << 4,
	M_CONF_REG_TYPE_INT64  = 1 << 5,
	M_CONF_REG_TYPE_UINT8  = 1 << 6,
	M_CONF_REG_TYPE_UINT16 = 1 << 7,
	M_CONF_REG_TYPE_UINT32 = 1 << 8,
	M_CONF_REG_TYPE_UINT64 = 1 << 9,
	M_CONF_REG_TYPE_BOOL   = 1 << 10,
	M_CONF_REG_TYPE_CUSTOM = 1 << 11,
} M_conf_reg_type_t;

/* Validator object. */
typedef struct M_conf_validator_wrap_t {
	M_conf_validator_t  cb;   /*!< Validator callback. */
	void               *data; /*!< Data to pass to the validator. */
} M_conf_validator_wrap_t;

/* Registration object. */
typedef struct M_conf_reg_t {
	M_conf_reg_type_t                           type;        /*!< Type of registration. */
	char                                       *key;         /*!< Key to register under. */
	union {
		char      *buf;
		char     **strdup;
		M_int8    *int8;
		M_int16   *int16;
		M_int32   *int32;
		M_int64   *int64;
		M_uint8   *uint8;
		M_uint16  *uint16;
		M_uint32  *uint32;
		M_uint64  *uint64;
		M_bool    *boolean;
	}                                           mem;         /*!< Memory address where value will be stored. */
	size_t                                      buf_len;     /*!< Size of buffer, for char [] registrations only. */
	union {
		char     *buf;
		char     *strdup;
		M_int8    int8;
		M_int16   int16;
		M_int32   int32;
		M_int64   int64;
		M_uint8   uint8;
		M_uint16  uint16;
		M_uint32  uint32;
		M_uint64  uint64;
		M_bool    boolean;
	}                                           default_val; /*!< Default value, used if ini file does not have a value for this key. */
	char                                       *regex;       /*!< Regular expression, for string registrations only. */
	M_int64                                     min_sval;    /*!< Minimum allowed value, for signed integer registrations only. */
	M_int64                                     max_sval;    /*!< Maximum allowed value, for signed integer registrations only. */
	M_uint64                                    min_uval;    /*!< Minimum allowed value, for unsigned integer registrations only. */
	M_uint64                                    max_uval;    /*!< Maximum allowed value, for unsigned integer registrations only. */
	union {
		M_conf_converter_buf_t    buf;
		M_conf_converter_strdup_t strdup;
		M_conf_converter_int8_t   int8;
		M_conf_converter_int16_t  int16;
		M_conf_converter_int32_t  int32;
		M_conf_converter_int64_t  int64;
		M_conf_converter_uint8_t  uint8;
		M_conf_converter_uint16_t uint16;
		M_conf_converter_uint32_t uint32;
		M_conf_converter_uint64_t uint64;
		M_conf_converter_bool_t   boolean;
		M_conf_converter_custom_t custom;
	}                                           converter;   /*!< Converter callback. */
} M_conf_reg_t;

/* Main conf object */
struct M_conf_t {
	char            *ini_path;       /*!< Path to ini file. */
	M_ini_t         *ini;            /*!< Object built from ini file. */
	M_list_t        *registrations;  /*!< List of key registration. */
	M_list_t        *validators;     /*!< List of validator callbacks for calling after M_conf_parse(). */
	M_list_t        *debug_loggers;  /*!< List of logging callbacks for logging debug messages. */
	M_list_t        *error_loggers;  /*!< List of logging callbacks for logging error messages. */
	M_hash_stru64_t *unused_keys;    /*!< Hash table for keeping count of unused keys. */
	M_bool           allow_multiple; /*!< Whether or not multiple keys are allowed. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#endif /* __M_CONF_INT_H__ */
