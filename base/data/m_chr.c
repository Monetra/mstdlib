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

#include "m_config.h"

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_chr_iscntrl(char c)
{
	return (c >= 0x00 && c <= 0x1f) || c == 0x7f;
}

M_bool M_chr_isascii(char c)
{
	/* Due to char sometimes being unsigned, we need to emit different checks
	 * depending on the CHAR_MIN/CHAR_MAX */
#if CHAR_MIN == 0
	return (c <= 127)?M_TRUE:M_FALSE;
#else
	return (c >= 0)?M_TRUE:M_FALSE;
#endif
}

M_bool M_chr_isgraph(char c)
{
	return c >= '!' && c < 0x7f;
}

M_bool M_chr_isblank(char c)
{
	return c == ' ' || c == '\t';
}

M_bool M_chr_isspace(char c)
{
	return M_chr_isblank(c) || c == '\f' || c == '\n' || c == '\r' || c == '\v';
}

M_bool M_chr_isprint(char c)
{
	/* Same as below, but without the call overhead
	 * return M_chr_isgraph(c) || M_chr_isspace(c); */
	return (c >= '!' && c < 0x7f) || ( c >= '\t' && c <= '\r') /* \t, \n, \v, \f, \r */ || c == ' ';
}

M_bool M_chr_islower(char c)
{
	return c >= 'a' && c <= 'z';
}

M_bool M_chr_isupper(char c)
{
	return c >= 'A' && c <= 'Z';
}

M_bool M_chr_isalpha(char c)
{
	return M_chr_islower(c) || M_chr_isupper(c);
}

M_bool M_chr_isalphasp(char c)
{
	return M_chr_islower(c) || M_chr_isupper(c) || c == ' ';
}

M_bool M_chr_isdigit(char c)
{
	return c >= '0' && c <= '9';
}

M_bool M_chr_isdec(char c)
{
	return (c >= '0' && c <= '9') || c == '.';
}

M_bool M_chr_isalnum(char c)
{
	return M_chr_isalpha(c) || M_chr_isdigit(c);
}

M_bool M_chr_isalnumsp(char c)
{
	return M_chr_isalpha(c) || M_chr_isdigit(c) || c == ' ';
}

M_bool M_chr_ispunct(char c)
{
	return !M_chr_isalnum(c) && M_chr_isgraph(c);
}

M_bool M_chr_ishex(char c)
{
	return M_chr_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

char M_chr_toupper(char c)
{
	return (char)(M_chr_islower(c) ? c - 32 : c);
}

char M_chr_tolower(char c)
{
	return (char)(M_chr_isupper(c) ? c + 32 : c);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_chr_digit(char c)
{
	return M_chr_isdigit(c) ? c - '0': -1;
}

/* - - */

/* assumes lowercase hex digit */
static int M_chr_xdigit_int(char c)
{
	if (M_chr_islower(c)) {
		return c-'a'+10;
	}
	return M_chr_digit(c);
}

int M_chr_xdigit(char c)
{
	if (M_chr_ishex(c)) {
		return M_chr_xdigit_int(M_chr_tolower(c));
	}
	return -1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
