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

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Find closing bracket for pattern range, honor escaping, and 
 * ignore closing bracket as first character or second character
 * if after '!' */
static const char *pattern_range_close(const char *pat)
{
	size_t i;
	size_t len;
	M_bool escaped = M_FALSE;

	len = M_str_len(pat);
	for (i=0; i<len; i++) {
		switch (pat[i]) {
			case '\\':
				if (escaped) {
					escaped = M_FALSE;
				} else {
					escaped = M_TRUE;
				}
			break;
			case ']':
				if (!(i == 0 || (i == 1 && pat[0] == '!')) && !escaped)
					return pat+i;
				escaped = M_FALSE;
			break;
			default:
				escaped = M_FALSE;
			break;
		}
	}
	return NULL;
}

/* Match pattern (pat) starting at pat_start to string (str) starting at str_start */
static M_bool pattern_matches(const char *pat, const char *s, size_t pat_start, size_t str_start, M_bool casecmp)
{
	char        c;
	char        c2;
	char        c3;
	size_t      i;
	size_t      pat_len;
	size_t      str_len;
	const char *rb       = NULL;
	M_bool      not_pat  = M_FALSE;

	if (pat == NULL || s == NULL)
		return M_FALSE;

	pat_len = M_str_len(pat);
	str_len = M_str_len(s);

	/* If done with both pattern and string, then it's a match, but
	 * if pattern is done, but string is not, fail */
	if (pat_start == pat_len)
		return (str_len == str_start) ? M_TRUE : M_FALSE;

	c = pat[pat_start];
	switch (c) {
		case '\\':
			if (pat_len == pat_start+1)
				return M_FALSE; /* Error */

			c2 = pat[pat_start+1];
			if (c2 == '[' || c2 == ']' || c2 == '?' || c2 == '*' || c2 == '\\') {
				if (str_len == str_start)
					return M_FALSE; /* String used up */
				if (s[str_start] == c2)
					return pattern_matches(pat, s, pat_start+2, str_start+1, casecmp);
			} else {
				return M_FALSE; /* Error */
			}
		break;
		case '[':
			if (str_len == str_start)
				return M_FALSE; /* Nothing to match */
			rb = pattern_range_close(pat+(pat_start+1)); /* Closing right bracket */
			if (rb == NULL)
				return M_FALSE; /* Error in pattern */
			/* If first character is '!' match everything but */
			if (pat[pat_start+1] == '!') {
				pat_start++;
				not_pat = M_TRUE;
			} else {
				not_pat = M_FALSE;
			}
			for (i=pat_start+1; i<(size_t)(rb-pat); i++) {
				c2 = pat[i];
				switch (c2) {
					case '\\':
						c3 = pat[i+1];
						if (c3 == '[' || c3 == ']' || c3 == '?' || c3 == '*' || c3 == '\\') {
							if (s[str_start] == c3) {
								if (not_pat)
									return M_FALSE;
								return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
							}
						} else {
							return M_FALSE; /* Error in pattern */
						}
					break;
					case '-':
						if (i == pat_start+1 || i == (size_t)(rb-pat)-1) {
							if (s[str_start] == '-') { /* First char or last in brackets */
								if (not_pat)
									return M_FALSE;
								return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
							}
						} else {
							/* Check Range */
							if (pat[i-1] < s[str_start] && s[str_start] < pat[i+1]) {
								if (not_pat)
									return M_FALSE;
								return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
							}
						}
					break;
					case ']':
						if (i == pat_start+1) {
							if (s[str_start] == c2) {
								if (not_pat)
									return M_FALSE;
								return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
							}
						} else {
							return M_FALSE;
						}
					break;
					default:
						if ((casecmp && M_chr_tolower(s[str_start]) == M_chr_tolower(c2)) || (!casecmp && s[str_start] == c2)) {
							if (not_pat)
								return M_FALSE;
							return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
						}
					break;
				}
			}
			/* Nothing matched */
			if (not_pat)
				return pattern_matches(pat, s, (size_t)(rb-pat)+1, str_start+1, casecmp);
			return M_FALSE;
		break;
		case ']':
			return M_FALSE; /* Error in pattern */
		break;
		case '?':
			if (str_len == str_start)
				return M_FALSE; /* String used up */
			/* ? always matches 1 char */
			return pattern_matches(pat, s, pat_start+1, str_start+1, casecmp);
		break;
		case '*':
			if (pat_start == pat_len-1) /* * matches anything */
				return M_TRUE;
			for (i=str_start; i<=str_len; i++) {
				/* Cycle through characters until we get a full pattern match */
				if (pattern_matches(pat, s, pat_start+1, i, casecmp))
					return M_TRUE;
			}
			/* No pattern match */
			return M_FALSE;
		break;
		default:
			if (str_len == str_start) {
				return M_FALSE; /* String used up */
			} else if ((casecmp && M_chr_tolower(s[str_start]) == M_chr_tolower(c)) || (!casecmp && s[str_start] == c)) {
				return pattern_matches(pat, s, pat_start+1, str_start+1, casecmp);
			} else {
				return M_FALSE;
			}
		break;
	}
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Match pattern against string as per  'man 7 glob' though we
 * don't support newer POSIX functions like named character classes
 * (e.g. [:lower:]), collating symbols, or equivalence class expressions */
M_bool M_str_pattern_match(const char *pattern, const char *s)
{
	return pattern_matches(pattern, s, 0, 0, M_FALSE);
}

M_API M_bool M_str_case_pattern_match(const char *pattern, const char *s)
{
	return pattern_matches(pattern, s, 0, 0, M_TRUE);
}
