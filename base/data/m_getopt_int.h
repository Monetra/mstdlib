/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_GETOPT_INT_H__
#define __M_GETOPT_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

typedef enum {
	M_GETOPT_TYPE_UNKNOWN = 0, /*!< An invalid type. */
	M_GETOPT_TYPE_INTEGER,     /*!< Integer type. */
	M_GETOPT_TYPE_DECIMAL,     /*!< Decimal type. */
	M_GETOPT_TYPE_STRING,      /*!< String type. */
	M_GETOPT_TYPE_BOOLEAN      /*!< Boolean type. */
} M_getopt_type_t;

struct M_getopt_option {
	M_getopt_type_t  type;
	char             short_opt;
	char            *long_opt;
	char            *description;
	M_bool           val_required;
	/* Callbacks for the various option types. */
	union {
		M_getopt_integer_cb integer_cb;
		M_getopt_decimal_cb decimal_cb;
		M_getopt_string_cb  string_cb;
		M_getopt_boolean_cb boolean_cb;
	} cb;
	struct M_getopt_option *next;
};
typedef struct M_getopt_option M_getopt_option_t;

struct M_getopt {
	char               *description; /*!< Information about the application. */
	M_getopt_option_t  *first;       /*!< First option in linked list of options. */
	M_getopt_option_t  *last;        /*!< Last option in linked list of options. */
	M_hash_strvp_t     *long_opts;   /*!< long_opt = M_getopt_option_t. */
	M_hash_u64vp_t     *short_opts;  /*!< short_opt = M_getopt_option_t. */
	M_getopt_nonopt_cb  nonopt_cb;   /*!< Non option cb. */
};

__END_DECLS

#endif /* __M_GETOPT_INT_H__ */
