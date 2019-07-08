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

#include <assert.h>
#include <stdlib.h> /* atoi; */
#include <string.h> /* strlen */

#ifdef HAVE_STRINGS_H
#  include <strings.h> /* strcasecmp, strncasecmp */
#endif

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * TODO:
 *
 *  size_t  M_str_cat_max(char *dst, size_t dst_len, const char *src, size_t src_len);
 */

M_BEGIN_IGNORE_DEPRECATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static char *M_strdup_int(const char *s, size_t n)
{
	char *ret;

	if (s == NULL)
		return NULL;

	ret = M_malloc(n+1);
	M_mem_copy(ret, s, n);
	ret[n] = '\0';

	return ret;
}

/* Constant time character to lower function. */
static char M_ct_tolower(volatile char c)
{
	volatile int  r   = 0;
	volatile int  q   = 0;
	volatile int  d   = 0;
	int           ret = 0;

	if (c < 'A')
		r++;
	if (c > '@')
		q++;
	if (c < '[')
		q++;
	if (c > 'Z')
		r++;

	c += 32;
	d  = c - 32;
	if (q == 2)
		ret = c;
	if (q != 2)
		ret = d;

	return (char)ret;
}

static M_bool M_str_eq_max_int(const char *s1, const char *s2, volatile size_t max, M_bool case_insensitive)
{
	/* NOTE: Constant-time implementation!
	 *
	 * This will always scan until the end of s1 (or max if set). The only
	 * timing knowledge that should be gained from this function is the length
	 * of s1 which is the user known input. This isn't an issue because they
	 * already know the length of the data they're providing.
	 *
	 *
	 * Compilers can optimize for true branch in if..else statements so else is
	 * never used. Ternary's count as if..else. Sames goes for && and ||
	 * because those can stop evaluation early.
	 *
	 * All set operations are balanced. Dummy vars are used to ensure there is
	 * always the same number of set operations and the same type.
	 *
	 * volatile is used to prevent compilers from optimizing certain variables.
	 * For example, dummy vars that are set but never used.
	 */
	char                 result = 0;
	size_t               i      = 0;
	volatile size_t      j      = 0;
	volatile size_t      k      = 0;
	M_bool               ret;
	volatile const char *sc     = NULL;

	/* Set the input to an empty string if it's NULL.
	 * This allows us to treat NULL as an empty string. */
	if (s1 == NULL)
		s1 = "";
	if (s1 != NULL)
		sc = "";
	if (s2 == NULL)
		s2 = "";
	if (s2 != NULL)
		sc = "";

	/* If max is zero, it means we want to scan the entire address range.
	 * Meaning until the end of s1. Callers shouldn't be setting a
	 * max past the end of s1 but if they do, we'll scan past the end
	 * of s1 which isn't good. We can only do so much. */
	i = max;
	if (max != 0)
		max = i;
	if (max == 0)
		max = SIZE_MAX;

	/* Constant time comparison.  We scan the entire string to prevent timing attacks.
	 * We don't want to do strlen()'s first that will leak info
	 * so we have a check for when we reach '\0'. */
	for (i=0; i<max; i++) {
		if (case_insensitive) {
			result |= (char)(M_ct_tolower(s1[i]) ^ M_ct_tolower(s2[j]));
		}
		if (!case_insensitive) {
			result |= s1[i] ^ s2[j];
		}

		if (s1[i] == '\0')
			break;

		if (s2[j] != '\0')
			j++;
		if (s2[j] == '\0')
			k++;
	}

	/* Normally you'd see: 'return result == 0'
	 * but we want to force M_TRUE and M_FALSE so we are
	 * 100% sure the true and false values match the defines. */
	if (result == 0)
		ret = M_TRUE;
	if (result != 0)
		ret = M_FALSE;
	return ret;
}

static M_bool M_str_eq_end_int(const char *s1, const char *s2, M_bool case_insensitive)
{
	size_t s1_len;
	size_t s2_len;
	size_t pos = 0;

	if (s1 == NULL || s2 == NULL)
		return M_FALSE;

	s1_len = M_str_len(s1);
	s2_len = M_str_len(s2);

	if (s2_len > s1_len)
		return M_FALSE;

	pos = s1_len-s2_len;
	return M_str_eq_max_int(s1+pos, s2, s2_len, case_insensitive);
}


static M_bool M_str_implode_has_restricted_chars(const char *s, unsigned char delim, unsigned char enclose_char, unsigned char escape_char)
{
	size_t i;
	size_t len;

	len = M_str_len(s);
	for (i=0; i<len; i++) {
		if (s[i] == delim|| s[i] == enclose_char || s[i] == escape_char) {
			return M_TRUE;
		}
	}
	return M_FALSE;
}

static void M_str_implode_escape(M_buf_t *buf, const char *s, unsigned char enclose_char, unsigned char escape_char)
{
	size_t len;
	size_t i;

	len = M_str_len(s);
	for (i=0; i<len; i++) {
		if (s[i] == enclose_char || s[i] == escape_char) {
			M_buf_add_byte(buf, escape_char);
		}

		M_buf_add_byte(buf, (unsigned char)s[i]);
	}
}

static char *M_str_map_max(char *s, size_t max, M_chr_map_func f)
{
	size_t i;

	if (s == NULL)
		return NULL;

	if (f == NULL)
		return s;

	for (i=0; i<max && s[i]!='\0'; i++)
		s[i] = f(s[i]);

	return s;
}

static M_bool is_escaped(const char *str, const char *str_pos, char escape)
{
	size_t escape_count;
	
	/* NOTE: an escape character may escape itself. So, we have to count the number of escape characters
	 *       before the current character, in order to determine if this character is escaped.
	 * 
	 * Ex: \\\" --> " is escaped by \
	 *     \\"  --> " is not escaped
	 *     \"   --> " is escaped by \
	 */
	
	/* Count the number of escapes before the current character. */
	escape_count = 0;
	while (str_pos > str && *(str_pos - 1) == escape) {
		escape_count++;
		str_pos--;
	}
	
	/* If the current char is preceeded by an odd number of escapes, this character is escaped. */
	return (escape_count % 2 == 1)? M_TRUE : M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_str_safe(const char *s)
{
	if (s == NULL)
		return "";
	return s;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Construction
 */

char *M_strdup(const char *s)
{
	if (s == NULL)
		return NULL;
	return M_strdup_int(s, M_str_len(s));
}

char *M_strdup_upper(const char *s)
{
	if (s == NULL)
		return NULL;
	return M_str_upper(M_strdup(s));
}

char *M_strdup_lower(const char *s)
{
	if (s == NULL)
		return NULL;

	return M_str_lower(M_strdup(s));
}

char *M_strdup_title(const char *s)
{
	return M_str_title(M_strdup(s));
}

char *M_strdup_trim(const char *s)
{
	if (s == NULL)
		return NULL;
	return M_str_trim(M_strdup(s));
}

char *M_strdup_unquote(const char *s, unsigned char quote, unsigned char escape)
{
	if (s == NULL)
		return NULL;
	return M_str_unquote(M_strdup(s), quote, escape);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_strdup_max(const char *s, size_t max)
{
	if (s == NULL)
		return NULL;
	return M_strdup_int(s, M_str_len_max(s, max));
}

char *M_strdup_upper_max(const char *s, size_t max)
{
	if (s == NULL)
		return NULL;
	return M_str_upper(M_strdup_max(s, max));
}

char *M_strdup_lower_max(const char *s, size_t max)
{
	if (s == NULL)
		return NULL;
	return M_str_lower(M_strdup_max(s, max));
}

char *M_strdup_title_max(const char *s, size_t max)
{
	return M_str_title(M_strdup_max(s, max));
}

char *M_strdup_trim_max(const char *s, size_t max)
{
	if (s == NULL)
		return NULL;
	return M_str_trim(M_strdup_max(s, max));
}

char *M_strdup_unquote_max(const char *s, unsigned char quote, unsigned char escape, size_t max)
{
	if (s == NULL)
		return NULL;
	return M_str_unquote(M_strdup_max(s, max), quote, escape);
}

char *M_strdup_justify(const char *src, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen)
{
	char   *str;
	size_t  len;

	if (M_str_isempty(src) || justlen == 0) {
		return NULL;
	}

	str = M_malloc(justlen + 1);
	len = M_str_justify(str, justlen + 1, src, justtype, justchar, justlen);

	if (len == 0) {
		M_free(str);
		return NULL;
	}

	return str;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Query
 */

M_bool M_str_isempty(const char *s)
{
	if (s == NULL || *s == '\0')
		return M_TRUE;
	return M_FALSE;
}

M_bool M_str_istrue(const char *s)
{
	if (s == NULL || *s == '\0')
		return M_FALSE;

	if (M_str_caseeq(s, "yes")  || M_str_caseeq(s, "y") || 
	    M_str_caseeq(s, "true") || M_str_caseeq(s, "t") ||
		M_str_caseeq(s, "1") || M_str_caseeq(s, "on"))
	{
		return M_TRUE;
	}
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_str_len(const char *s)
{
	if (s == NULL)
		return 0;
	return strlen(s);
}

size_t M_str_len_max(const char *s, size_t max)
{
	const char *t;

	if (s == NULL || max == 0)
		return 0;

	/* memchr (called by M_mem_chr) cannot handle SIZE_MAX */
	if (max == SIZE_MAX)
		return M_str_len(s);
	
	t = M_mem_chr(s, '\0', max);
	if (t == NULL)
		return max;
	return (size_t)(t-s);
}

/* - - - - - - - - - - - - - - - - - - - - */

char *M_str_chr(const char *s, char c)
{
	if (s == NULL)
		return NULL;
	return strchr(s, c);
}

char *M_str_rchr(const char *s, char c)
{
	if (s == NULL)
		return NULL;
	return strrchr(s, c);
}


char *M_str_find_first_from_charset(const char *str, const char *charset)
{
	if (M_str_isempty(str) || M_str_isempty(charset)) {
		return NULL;
	}
	return strpbrk(str, charset);
}


char *M_str_find_first_not_from_charset(const char *str, const char *charset)
{
	if (str == NULL) {
		return NULL;
	}
	str += strspn(str, M_str_safe(charset));
	if (*str == '\0') {
		return NULL;
	}
	return M_CAST_OFF_CONST(char *, str);
}


static __inline__ M_bool M_str_ispredicate_max_inline(const char *s, size_t max, M_chr_predicate_func pred)
{
	size_t i;

	/* XXX: REVISIT THIS LOGIC.
	 * Shouldn't this be M_FALSE? */
	if (s == NULL || max == 0)
		return M_TRUE;

	for (i=0; i<max && s[i] != '\0'; i++) {
		if (!pred(s[i])) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}


M_bool M_str_ispredicate(const char *s, M_chr_predicate_func pred)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, pred);
}

M_bool M_str_isalnum(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isalnum);
}

M_bool M_str_isalnumsp(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isalnumsp);
}

M_bool M_str_isalpha(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isalpha);
}

M_bool M_str_isalphasp(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isalphasp);
}

M_bool M_str_isspace(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isspace);
}

M_bool M_str_isascii(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isascii);
}

M_bool M_str_isgraph(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isgraph);
}

M_bool M_str_isprint(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isprint);
}

M_bool M_str_ishex(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_ishex);
}

M_bool M_str_isbase64(const char *s)
{
	return M_str_isbase64_max(s, M_str_len(s));
}

M_bool M_str_isnum(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isdigit);
}

M_bool M_str_isdec(const char *s)
{
	return M_str_ispredicate_max_inline(s, SIZE_MAX, M_chr_isdec);
}

M_bool M_str_ismoney(const char *s)
{
	M_bool seen_dot = M_FALSE;
	size_t dec_dot  = 0;
	size_t len;
	size_t i;

	if (s == NULL || *s == '\0')
		return M_FALSE;

	len = M_str_len(s);
	for (i=0; i<len; i++) {
		if (!M_chr_isdec(s[i])) {
			return M_FALSE;
		}

		if (s[i] == '.') {
			if (seen_dot) {
				return M_FALSE;
			}
			seen_dot = M_TRUE;
			continue;
		}

		if (seen_dot) {
			dec_dot++;
			if (dec_dot > 2) {
				return M_FALSE;
			}
		}
	}

	return M_TRUE;
}


M_bool M_str_ischarset(const char *str, const char *charset)
{
    /* TODO: Equivalent way using std C89 functions:
     
	if (M_str_isempty(str) || M_str_isempty(charset)) {
		return M_FALSE;
	}
	return (M_str_len(str) == strspn(str, charset))? M_TRUE : M_FALSE;
	
	*/
	
	/* Original implementation */
	char   a;
	char   c;
	M_bool ret;
	size_t len1;
	size_t len2;
	size_t i;
	size_t j;

	if (M_str_isempty(str) || M_str_isempty(charset))
		return M_FALSE;

	len1 = M_str_len(str);
	len2 = M_str_len(charset);
	for (i=0; i<len1; i++) {
		ret = M_FALSE;
		c   = str[i];
	
		for (j=0; j<len2; j++) {
			a = charset[j];
			if (c == a) {
				ret = M_TRUE;
				break;
			}
		}
	
		if (!ret) {
			return M_FALSE;
		}
	}
	return M_TRUE;
}

M_bool M_str_isnotcharset(const char *str, const char *charset)
{
	if (M_str_isempty(str) || M_str_isempty(charset)) {
		return M_TRUE;
	}
	return (M_str_len(str) == strcspn(str, charset))? M_TRUE : M_FALSE;
}


M_API M_bool M_str_isstr(const unsigned char *s, size_t len)
{
	if (s == NULL || len == 0)
		return M_FALSE;

	/* Check for NULL terminator */
	if (s[len-1] != '\0')
		return M_FALSE;

	/* Validate M_str_len() agrees on the length */
	if (M_str_len((const char *)s) != len-1)
		return M_FALSE;

	/* Zero-length string is still a string */
	if (len == 1)
		return M_TRUE;

	/* All data of string must be printable */
	return M_str_isprint((const char *)s);
}


M_bool M_str_ispredicate_max(const char *s, size_t max, M_chr_predicate_func pred)
{
	return M_str_ispredicate_max_inline(s, max, pred);
}

M_bool M_str_isalnum_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isalnum);
}

M_bool M_str_isalnumsp_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isalnumsp);
}

M_bool M_str_isalpha_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isalpha);
}

M_bool M_str_isalphasp_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isalphasp);
}

M_bool M_str_isspace_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isspace);
}

M_bool M_str_isgraph_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isgraph);
}

M_bool M_str_isprint_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isprint);
}

M_bool M_str_ishex_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_ishex);
}

M_bool M_str_isbase64_max(const char *s, size_t max)
{
	/* We're assuming that the wrap length (if there is any wrapping) is a multiple of 4. */
	M_bool         ret = M_FALSE;
	char         **lines;
	size_t         count;
	size_t        *lengths;
	unsigned int   i;
	unsigned int   j;

	if (M_str_isempty(s))
		return M_FALSE;

	lines = M_str_explode('\n', s, max, &count, &lengths);
	if (lines == NULL || count == 0)
		goto done;

	for (i=0; i<count; i++) {
		/* All lines except for the last must be wrapped to the same width. */
		if (lengths[i] != lengths[0] && i < count-1)
			goto done;

		for (j=0; j<lengths[i]; j++) {
			if (lines[i][j] == '=') {
				/* Only the last line can have padding, and only as the last (two) character(s) of that line. */
				if (i != count-1 || j < lengths[i]-2)
					goto done;

				/* If the second-to-last character is a '=', the last one must be as well. */
				if (j == lengths[i]-2 && lines[i][j+1] != '=')
					goto done;
			} else {
				if (!M_chr_isalnum(s[i]) && s[i] != '+' && s[i] != '/')
					goto done;
			}
		}
	}

	ret = M_TRUE;
done:
	M_str_explode_free(lines, count);
	M_free(lengths);

	return ret;
}

M_bool M_str_isnum_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isdigit);
}

M_bool M_str_isdec_max(const char *s, size_t max)
{
	return M_str_ispredicate_max_inline(s, max, M_chr_isdec);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_str_str(const char *haystack, const char *needle)
{
	if (haystack == NULL)
		return NULL;

	if (needle == NULL)
		return M_CAST_OFF_CONST(char *, haystack);

	return strstr(haystack, needle);
}

char *M_str_casestr(const char *haystack, const char *needle)
{
	size_t i;
	size_t len;
	size_t nlen;

	if (haystack == NULL || needle == NULL)
		return NULL;

	len  = M_str_len(haystack);
	nlen = M_str_len(needle);
	for (i=0; i<len; i++) {
		if (nlen > len - i)
			break;
		if (M_str_caseeq_max(haystack+i, needle, nlen))
			return (char *)((M_uintptr)haystack+i);
	}
	return NULL;
}

ssize_t M_str_casestr_pos(const char *haystack, const char *needle)
{
	char *temp = NULL;

	temp = M_str_casestr(haystack, needle);

	if (temp == NULL)
		return -1;

	return (ssize_t)(temp - haystack);
}


int M_str_cmpsort(const char *s1, const char *s2)
{
	if (s1 == s2)
		return 0;
	return strcmp(M_str_safe(s1), M_str_safe(s2));
}


int M_str_cmpsort_max(const char *s1, const char *s2, size_t max)
{
	if (s1 == s2)
		return 0;
	return strncmp(M_str_safe(s1), M_str_safe(s2), max);
}

#if defined(HAVE_STRCASECMP)
#  define M_str_casecmp_unsafe strcasecmp
#elif defined(HAVE__STRICMP)
#  define M_str_casecmp_unsafe _stricmp
#else
#  error No usable M_str_casecmp_unsafe implementation!
#endif

int M_str_casecmpsort(const char *s1, const char *s2)
{
	if (s1 == s2)
		return 0;
	return M_str_casecmp_unsafe(M_str_safe(s1), M_str_safe(s2));
}

#if defined(HAVE_STRNCASECMP)
#  define M_str_casecmp_max_unsafe strncasecmp
#elif defined(HAVE__STRNICMP)
#  define M_str_casecmp_max_unsafe _strnicmp
#else
#  error No usable M_str_casecmp_max_unsafe implementation!
#endif

int M_str_casecmpsort_max(const char *s1, const char *s2, size_t max)
{
	if (s1 == s2)
		return 0;
	return M_str_casecmp_max_unsafe(M_str_safe(s1), M_str_safe(s2), max);
}

M_bool M_str_eq_max(const char *s1, const char *s2, size_t max)
{
	return M_str_eq_max_int(s1, s2, max, M_FALSE);
}

M_bool M_str_eq(const char *s1, const char *s2)
{
	return M_str_eq_max(s1, s2, 0);
}

M_bool M_str_caseeq_max(const char *s1, const char *s2, size_t max)
{
	return M_str_eq_max_int(s1, s2, max, M_TRUE);
}

M_bool M_str_caseeq(const char *s1, const char *s2)
{
	return M_str_caseeq_max(s1, s2, 0);
}

M_bool M_str_eq_end(const char *s1, const char *s2)
{
	return M_str_eq_end_int(s1, s2, M_FALSE);
}

M_bool M_str_caseeq_end(const char *s1, const char *s2)
{
	return M_str_eq_end_int(s1, s2, M_TRUE);
}

M_bool M_str_eq_start(const char *s1, const char *s2)
{
	return M_str_eq_max_int(s1, s2, M_str_len(s2), M_FALSE);
}

M_bool M_str_caseeq_start(const char *s1, const char *s2)
{
	return M_str_eq_max_int(s1, s2, M_str_len(s2), M_TRUE);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Split
 */

char *M_str_split_on_char(char *s, char c)
{
	if (s == NULL || c == '\0')
		return NULL;

	for (; *s != '\0' && *s != c; s++)
		;

	if (*s != '\0')
		*s++ = '\0';

	return s;
}


char **M_str_explode(unsigned char delim, const char *s, size_t s_len, size_t *num, size_t **len_array)
{
	size_t          i;
	size_t          num_strs = 1;
	unsigned char **out      = NULL;
	unsigned char  *dupstr   = NULL;

	/* num is required but we want everything initialized
	 * that can be before we fail sanity checks. */
	if (num != NULL)
		*num = 0;
	if (len_array != NULL)
		*len_array = NULL;

	if (s == NULL || *s == '\0' || num == NULL)
		return NULL;

	/* Duplicate memory and make sure there is a NULL terminator in case we're
	 * dealing with string data */
	dupstr = M_malloc(s_len + 1);
	if (s_len > 0)
		M_mem_copy(dupstr, s, s_len);
	dupstr[s_len] = 0;

	/* Count number of delimiters to get number of output sections */
	for (i=0; i<s_len; i++) {
		if (dupstr[i] == delim)
			num_strs++;
	}

	*num = num_strs;
	out  = M_malloc(num_strs * sizeof(*out));
	if (len_array != NULL)
		*len_array = M_malloc(num_strs * sizeof(**len_array));

	out[0]   = dupstr;
	num_strs = 1;
	for (i=0; i<s_len; i++) {
		if (dupstr[i] != delim)
			continue;

		dupstr[i]     = 0;
		out[num_strs] = dupstr+(i+1);
		if (len_array != NULL)
			(*len_array)[num_strs-1] = (size_t)((dupstr+i) - out[num_strs-1]);

		num_strs++;
	}

	if (len_array != NULL)
		(*len_array)[num_strs-1] = (size_t)((dupstr+s_len) - out[num_strs-1]);

	return (char **)out;
}

char **M_str_explode_str(unsigned char delim, const char *s, size_t *num)
{
	return M_str_explode(delim, s, M_str_len(s), num, NULL);
}

char **M_str_explode_lines(size_t max_lines, size_t max_chars, const char *src_str, M_bool truncate, size_t *num)
{
	char    *dup_str;
	char   **out;
	size_t   src_len  = M_str_len(src_str);
	size_t   line_num;

	if (max_lines == 0 || max_chars == 0 || src_str == NULL || num == NULL)
		return NULL;

	/* Worst case: solid block of max_lines*max_chars non-whitespace text. In this case, we'd need to add a '\0' at
	 *             the end of each line, and there aren't any spaces to consume. So, we need to allocate enough space
	 *             to store the entire input string, plus an extra char for each line.
	 */
	dup_str = M_malloc_zero(src_len + max_lines);
	out     = M_malloc_zero(max_lines * sizeof(*out));

	line_num = 0;
	while (line_num < max_lines) {
		size_t chunk_sz; /* size of chunk we're reading into dupstr. */
		size_t copy_sz;

		out[line_num] = dup_str;

		/* Remove any leading spaces from the source string. */
		while (M_chr_isspace(*src_str)) {
			src_str++;
			src_len--;
		}

		/* Figure out the size of the chunk we want to display on this line. */
		if (src_len == 0) {
			break;
		} else if (src_len <= max_chars) {
			chunk_sz = src_len;
		} else {
			chunk_sz = max_chars;
			/* If breaking at max_chars would divide a word, try to break on whitespace before start of last word. */
			while (chunk_sz > 0 && !M_chr_isspace(src_str[chunk_sz])) {
				chunk_sz--;
			}
			/* If we have a single word that's longer than the maximum line length, only option is to break it up. */
			if (chunk_sz == 0) {
				chunk_sz = max_chars;
			}
		}

		/* Copy current line into dupstr. Remove any additional trailing whitespace. */
		copy_sz = chunk_sz;
		while (copy_sz > 0 && M_chr_isspace(src_str[copy_sz - 1])) {
			copy_sz--;
		}
		M_mem_copy(dup_str, src_str, copy_sz);
		dup_str[copy_sz] = '\0';
		dup_str         += copy_sz + 1;  /* dupstr now points to next empty spot after \0. */

		/* Consume bytes from source string. */
		src_str += chunk_sz; /* srcstr now points to first char after the chunk we copied. */
		src_len -= chunk_sz;

		line_num++;
	}

	*num = line_num;

	/* If we aren't outputting any lines, return NULL. */
	if (*num == 0) {
		M_free(dup_str);
		M_free(out);
		return NULL;
	}

	/* If truncate is set to false, leaving info out is an error condition. */
	if (!truncate) {
		/* Remove leading spaces from the source string. */
		while (M_chr_isspace(*src_str)) {
			src_str++;
			src_len--;
		}
		/* If we have non-whitespace chars left in the string, it's an error. */
		if (src_len > 0) {
			M_str_explode_free(out, *num);
			*num = 0;
			return NULL;
		}
	}

	return out;
}

char **M_str_explode_quoted(unsigned char delim, const char *s, size_t s_len, unsigned char quote_char, unsigned char escape_char, size_t max_sects, size_t *num, size_t **len_array)
{
	size_t          i;
	size_t          num_strs  = 0;
	size_t          beginsect = 0;
	char          **out       = NULL;
	char           *dupstr    = NULL;
	M_bool          on_quote;

	*num = 0;

	if (len_array)
		*len_array = NULL;

	if (s == NULL || s_len == 0)
		return NULL;

	/* Duplicate memory and make sure there is a NULL terminator incase we're
	 * dealing with string data */
	dupstr = M_malloc(s_len + 1);
	if (s_len > 0)
		M_mem_copy(dupstr, s, s_len);
	dupstr[s_len] = 0;

	/* Count number of delimiters to get number of output sections */
	on_quote = M_FALSE;
	for (i=0; i<s_len; i++) {
		if (quote_char != 0 && dupstr[i] == quote_char) {
			/* Doubling the quote char acts as escaping */
			if (quote_char == escape_char && dupstr[i+1] == quote_char) {
				i++;
				continue;
			} else if (escape_char != 0 && quote_char != escape_char && i > 0 && dupstr[i-1] == escape_char) {
				continue;
			} else if (on_quote) {
				on_quote = M_FALSE;
			} else {
				on_quote = M_TRUE;
			}
		}
		if (dupstr[i] == delim && !on_quote) {
			num_strs++;
			beginsect = i+1;
			if (max_sects != 0 && num_strs == max_sects-1) {
				break;
			}
		}
	}
	if (beginsect <= s_len)
		num_strs++;

	/* Create the array to hold our exploded parts */
	*num = num_strs;
	out = M_malloc(num_strs * sizeof(*out));

	if (len_array != NULL)
		*len_array = M_malloc(num_strs * sizeof(**len_array));

	/* Fill in the array with pointers to the exploded parts */
	num_strs  = 0;
	beginsect = 0;
	on_quote  = M_FALSE;
	for (i=0; i<s_len; i++) {
		if (quote_char != 0 && dupstr[i] == quote_char) {
			/* Doubling the quote char acts as escaping */
			if (quote_char == escape_char && dupstr[i+1] == quote_char) {
				i++;
				continue;
			} else if (escape_char != 0 && quote_char != escape_char && i > 0 && dupstr[i-1] == escape_char) {
				continue;
			} else if (on_quote) {
				on_quote = M_FALSE;
			} else {
				on_quote = M_TRUE;
			}
		}
		if (dupstr[i] == delim && !on_quote) {
			dupstr[i]       = 0;
			out[num_strs++] = dupstr+beginsect;
			if (len_array != NULL)
				(*len_array)[num_strs-1] = (size_t)((dupstr+i) - out[num_strs-1]);
			beginsect       = i+1;
			if (max_sects != 0 && num_strs == max_sects-1) {
				break;
			}
		}
	}
	if (beginsect <= s_len) {
		out[num_strs++] = dupstr+beginsect;
		if (len_array != NULL)
			(*len_array)[num_strs-1] = (size_t)((dupstr+s_len) - out[num_strs-1]);
	}

	return out;
}

char **M_str_explode_str_quoted(unsigned char delim, const char *s, unsigned char quote_char, unsigned char escape_char, size_t max_sects, size_t *num)
{
	return M_str_explode_quoted(delim, s, M_str_len(s), quote_char, escape_char, max_sects, num, NULL);
}


int *M_str_explode_int(unsigned char delim, const char *s, size_t *num)
{
	char   **strs     = NULL;
	size_t   num_strs = 0;
	size_t   i;
	int     *out      = NULL;
	size_t   cnt      = 0;

	*num = 0;

	if (s == NULL || *s == '\0')
		return NULL;

	strs = M_str_explode_str(delim, s, &num_strs);
	if (strs == NULL) {
		M_str_explode_free(strs, num_strs);
		return NULL;
	}

	out = M_malloc(num_strs * sizeof(*out));
	for (i=0; i<num_strs; i++) {
		if (strs[i] == NULL)
			continue;

		M_str_trim(strs[i]);
		if (*(strs[i]) == '\0') {
			continue;
		}

		out[cnt++] = M_str_to_int32(strs[i]);
	}

	M_str_explode_free(strs, num_strs);
	if (cnt == 0) {
		M_free(out);
		out = NULL;
	}

	*num = cnt;
	return out;
}

void M_str_explode_free(char **strs, size_t num)
{
	if (strs != NULL) {
		/* First pointer contains entire buffer */
		if (num > 0) M_free(strs[0]);
		M_free(strs);
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Join
 */

char *M_str_implode(unsigned char delim, unsigned char enclose_char, unsigned char escape_char, char **strs, size_t num_strs, M_bool always_enclose)
{
	M_buf_t *buf;
	size_t   outlen = 0;
	size_t   i;
	M_bool   has_restricted;

	if (num_strs == 0)
		return NULL;

	buf = M_buf_create();

	for (i=0; i<num_strs; i++) {
		if (i != 0) {
			M_buf_add_byte(buf, delim);
		}

		if (strs[i] == NULL) {
			has_restricted = M_FALSE;
		} else {
			has_restricted = M_str_implode_has_restricted_chars(strs[i], delim, enclose_char, escape_char);
		}

		if (always_enclose || has_restricted) {
			M_buf_add_byte(buf, enclose_char);
			if (has_restricted) {
				M_str_implode_escape(buf, strs[i], enclose_char, escape_char);
			} else if (strs[i] != NULL) {
				M_buf_add_str(buf, strs[i]);
			}
			M_buf_add_byte(buf, enclose_char);
		} else if (strs[i] != NULL) {
			M_buf_add_str(buf, strs[i]);
		}
	}

	return M_buf_finish_str(buf, &outlen);
}

char *M_str_implode_int(unsigned char delim, const int *ints, size_t num_ints)
{
	M_buf_t *buf;
	size_t   outlen = 0;
	size_t   i;

	if (num_ints == 0)
		return NULL;

	buf = M_buf_create();

	if (num_ints > 0) {
		M_buf_add_int(buf, ints[0]);
		for (i=1; i<num_ints; i++) {
			M_buf_add_byte(buf, delim);
			M_buf_add_int(buf, ints[i]);
		}
	}

	return M_buf_finish_str(buf, &outlen);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Mutation
 */


char *M_str_lower(char *s)
{
	return M_str_lower_max(s, SIZE_MAX);
}

char *M_str_lower_max(char *s, size_t max)
{
	return M_str_map_max(s, max, M_chr_tolower);
}

char *M_str_upper(char *s)
{
	return M_str_upper_max(s, SIZE_MAX);
}

char *M_str_upper_max(char *s, size_t max)
{
	return M_str_map_max(s, max, M_chr_toupper);
}

char *M_str_title(char *s)
{
	return M_str_title_max(s, SIZE_MAX);
}

char *M_str_title_max(char *s, size_t max)
{
	size_t len;
	size_t i;
	M_bool prev_white;

	len = M_str_len_max(s, max);

	prev_white = M_TRUE;
	for (i=0; i<len; i++) {
		if (prev_white) {
			s[i] = M_chr_toupper(s[i]);
		} else {
			s[i] = M_chr_tolower(s[i]);
		}
		prev_white = M_chr_isspace(s[i]);
	}

	return s;
}

char *M_str_trim(char *s)
{
	return M_str_trim_max(s, SIZE_MAX);
}

char *M_str_trim_max(char *s, size_t max)
{
	ssize_t i;
	size_t  len;

	if (s == NULL)
		return s;

	len = M_str_len_max(s, max);

	if (len == 0)
		return s;

	/* Find first non-whitespace */
	for (i=0; i < (ssize_t)len && M_chr_isspace(s[i]); i++);

	/* All whitespace */
	if (i == (ssize_t)len) {
		s[0] = 0;
		return s;
	}

	if (i != 0) {
		M_mem_move(s, s+i, len-(size_t)i);
		len -= (size_t)i;
		s[len] = 0;
	}

	/* Trim off ending whitespace */
	for (i=(ssize_t)len-1; i>=0 && M_chr_isspace(s[i]); i--);

	/* Terminate */
	s[i+1] = '\0';

	return s;
}


char *M_str_remove_bracketed(const char *src, char open, char close)
{
	return M_str_remove_bracketed_quoted(src, open, close, '\0', '\0');
}


char *M_str_remove_bracketed_quoted(const char *src, char open, char close, char quote, char escape)
{
	char       *dst;
	char       *dst_pos;
	const char *src_pos;
	M_bool      in_quotes  = M_FALSE;
	size_t      open_count = 0;
	
	
	dst     = M_malloc_zero(M_str_len(src) + 1);
	dst_pos = dst;
	src_pos = src;
	
	if (open == close || open == '\0' || close == '\0' || M_str_isempty(src)) {
		M_free(dst);
		return NULL;
	}
	
	while (*src_pos != '\0') {
		/* Handle quote. */
		if (*src_pos == quote && !is_escaped(src, src_pos, escape)) {
			in_quotes = !in_quotes; /* toggle */
		}
		
		/* Handle open bracket. */
		if (*src_pos == open && !in_quotes) {
			open_count++;
		}
		
		/* Copy characters outside bracketed expressions into destination. */
		if (open_count == 0) {
			*dst_pos = *src_pos;
			dst_pos++;
		}
		
		/* Handle close bracket. */
		if (*src_pos == close && !in_quotes) {
			if (open_count == 0) {
				/* Error - close bracket without matching open bracket. */
				M_free(dst);
				return NULL;
			}
			open_count--;
		}
		
		src_pos++;
	}
	
	if (open_count > 0) {
		/* Error - open bracket without matching close bracket. */
		M_free(dst);
		return NULL;
	}
	
	if (in_quotes) {
		/* Error - missing closing quote. */
		M_free(dst);
		return NULL;
	}
	
	return dst;
}


char *M_str_keep_bracketed(const char *src, char open, char close)
{
	return M_str_keep_bracketed_quoted(src, open, close, '\0', '\0');
}


char *M_str_keep_bracketed_quoted(const char *src, char open, char close, char quote, char escape)
{
	char       *dst;
	char       *dst_pos;
	const char *src_pos;
	M_bool      in_quotes  = M_FALSE;
	size_t      open_count = 0;
	
	
	dst     = M_malloc_zero(M_str_len(src) + 1);
	dst_pos = dst;
	src_pos = src;
	
	if (open == close || open == '\0' || close == '\0' || M_str_isempty(src)) {
		M_free(dst);
		return NULL;
	}
	
	while (*src_pos != '\0') {
		/* Handle quote. */
		if (*src_pos == quote && !is_escaped(src, src_pos, escape)) {
			in_quotes = !in_quotes; /* toggle */
		}
		
		/* Handle close bracket. */
		if (*src_pos == close && !in_quotes) {
			if (open_count == 0) {
				/* Error - close bracket without matching open bracket. */
				M_free(dst);
				return NULL;
			}
			open_count--;
		}
		
		/* Copy characters inside bracketed expressions into destination. The angle brackets from the top-level
		 * bracketed expression are not included in the output, but those from nested brackets are included.
		 */
		if (open_count > 0) {
			*dst_pos = *src_pos;
			dst_pos++;
		}
		
		/* Handle open bracket. */
		if (*src_pos == open && !in_quotes) {
			open_count++;
		}
		
		src_pos++;
	}
	
	if (open_count > 0) {
		/* Error - open bracket without matching close bracket. */
		M_free(dst);
		return NULL;
	}
	
	if (in_quotes) {
		/* Error - missing closing quote. */
		M_free(dst);
		return NULL;
	}
	
	return dst;
}


char *M_str_remove_quoted(const char *src, char quote, char escape)
{
	char       *dst;
	char       *dst_pos;
	const char *src_pos;
	M_bool      in_quotes = M_FALSE;
	
	
	dst     = M_malloc_zero(M_str_len(src) + 1);
	dst_pos = dst;
	src_pos = src;
	
	if (M_str_isempty(src)) {
		M_free(dst);
		return NULL;
	}
	
	while (*src_pos != '\0') {
		if (*src_pos == quote && !is_escaped(src, src_pos, escape)) {
			in_quotes = !in_quotes; /* toggle */
		} else if (!in_quotes) {
			/* Add the current character to the destination string. */
			if (*src_pos == quote || (*src_pos == escape && is_escaped(src, src_pos, escape))) {
				/* If the char we're adding is an escaped quote or escape, remove escape char by overwriting
				 * it instead of writing to the next open spot in dst.
				 */
				*(dst_pos - 1) = *src_pos;
			} else {
				/* Write all other chars to the next open spot in dst and advance. */
				*dst_pos = *src_pos;
				dst_pos++;
			}
		}
		
		src_pos++;
	}
	
	if (in_quotes) {
		/* Error - quoted section wasn't closed. */
		M_free(dst);
		return NULL;
	}
	
	return dst;
}


char *M_str_keep_quoted(const char *src, char quote, char escape)
{
	M_bool      in_quotes = M_FALSE;
	char       *dst;
	char       *dst_pos;
	const char *src_pos;
	
	
	dst     = M_malloc_zero(M_str_len(src) + 1);
	dst_pos = dst;
	src_pos = src;
	
	if (M_str_isempty(src)) {
		M_free(dst);
		return NULL;
	}
	
	while (*src_pos != '\0') {
		if (*src_pos == quote && !is_escaped(src, src_pos, escape)) {
			in_quotes = !in_quotes; /* toggle */
		} else if (in_quotes) {
			/* Add the current character to the destination string. */
			if (*src_pos == quote || (*src_pos == escape && is_escaped(src, src_pos, escape))) {
				/* If the char we're adding is an escaped quote or escape, remove escape char by overwriting
				 * it instead of writing to the next open spot in dst.
				 */
				*(dst_pos - 1) = *src_pos;
			} else {
				/* Write all other chars to the next open spot in dst and advance. */
				*dst_pos = *src_pos;
				dst_pos++;
			}
		}
		
		src_pos++;
	}
	
	if (in_quotes) {
		/* Error - quoted section wasn't closed. */
		M_free(dst);
		return NULL;
	}
	
	return dst;
}


char *M_str_unquote(char *s, unsigned char quote, unsigned char escape)
{
	return M_str_unquote_max(s, quote, escape, SIZE_MAX);
}

char *M_str_unquote_max(char *s, unsigned char quote, unsigned char escape, size_t max)
{
	size_t i;
	size_t len;
	
	if (s == NULL)
		return s;

	/* Trim any whitespace from the string. */
	s = M_str_trim_max(s, max);

	/* All whitespace nothing to do. */
	if (*s == '\0')
		return s;

	len = M_str_len_max(s, max);
	if (len == 1)
		return s;

	/* String must start and end with a quote to be considered quoted. */
	if (*s != quote || s[len-1] != quote)
		return s;

	/* strip the start and end quotes */
	s[len-1] = '\0';
	len--;
	/* We want to move the \0 as well so the string is still NULL terminated. */
	M_mem_move(s, s+1, len);
	len--;
	
	/* remove the escaping character from internal escape and quote characters. */
	for (i=0; i<len; i++) {
		/* Remove escaping from escaped escape characters. */
		if ((unsigned char)s[i] == escape) {
			if (s[i+1] == escape || s[i+1] == quote) {
				M_mem_move(s+i, s+i+1, len-i);
				len--;
			}
		}
	}

	return s;
}

char *M_str_quote(const char *s, unsigned char quote, unsigned char escape)
{
	return M_str_quote_max(s, quote, escape, SIZE_MAX);
}

char *M_str_quote_max(const char *s, unsigned char quote, unsigned char escape, size_t max)
{
	M_buf_t *buf;
	size_t   i;
	size_t   len;
	size_t   start = 0;
	
	if (s == NULL)
		return NULL;

	if (*s == '\0')
		return M_strdup("");

	buf = M_buf_create();
	len = M_str_len_max(s, max);

	M_buf_add_byte(buf, quote);
	for (i=0; i<len; i++) {
		if (s[i] == quote || s[i] == escape) {
			M_buf_add_bytes(buf, s+start, i-start);
			M_buf_add_byte(buf, escape);
			start = i;
		}
	}

	if (start < len)
		M_buf_add_bytes(buf, s+start, len-start);

	M_buf_add_byte(buf, quote);
	return M_buf_finish_str(buf, &len);
}

M_bool M_str_quote_if_necessary(char **out, const char *s, unsigned char quote, unsigned char escape, unsigned char delim)
{
	size_t i;
	size_t len;

	if (out == NULL || M_str_isempty(s))
		return M_FALSE;

	*out = NULL;
	len  = M_str_len(s);

	if (*s == ' ' || s[len-1] == ' ') {
		*out = M_str_quote(s, quote, escape);
		return M_TRUE;
	}

	for (i=0; i<len; i++) {
		if (s[i] == quote || s[i] == delim) {
			*out = M_str_quote(s, quote, escape);
			return M_TRUE;
		}
	}

	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_str_cat(char *dest, size_t dest_len, const char *src)
{
	size_t dlen;
	size_t slen;

	if (dest == NULL || dest_len == 0 || src == NULL)
		return M_FALSE;

	dlen = M_str_len(dest);
	slen = M_str_len(src);
	if (dlen+slen >= dest_len)
		return M_FALSE;
	
	M_mem_copy(dest+dlen, src, slen);
	dest[dlen+slen] = '\0';

	return M_TRUE;
}

char *M_str_delete_spaces(char *s)
{
	size_t src;
	size_t dst;
	size_t len;

	if (s == NULL)
		return s;

	len = M_str_len(s);
	for (dst=src=0; src<len; src++) {
		if (!M_chr_isspace(s[src])) {
			s[dst++] = s[src];
		}
	}
	s[dst] = '\0';

	return s;
}

char *M_str_delete_newlines(char *s)
{
	size_t src;
	size_t dst;
	size_t len;

	if (s == NULL)
		return s;

	len = M_str_len(s);
	for (dst=src=0; src<len; src++) {
		if (s[src] != '\n' && s[src] != '\r') {
			s[dst++] = s[src];
		}
	}
	s[dst] = '\0';

	return s;
}

char *M_str_replace_chr(char *s, char b, char a)
{
	size_t len;
	size_t i;

	if (s == NULL || *s == '\0')
		return s;

	len = M_str_len(s);
	for (i=0; i<len; i++) {
		if (s[i] == b) {
			s[i] = a;
		}
	}

	return s;
}

char *M_strdup_replace_charset(const char *s, const unsigned char *bcs, size_t bcs_len, const char *a)
{
	M_buf_t *buf;
	size_t   len;
	size_t   i;
	size_t   j;

	if (s == NULL)
		return NULL;
	if (*s == '\0')
		return M_strdup(s);

	buf = M_buf_create();
	len = M_str_len(s);
	for (i=0; i<len; i++) {
		M_buf_add_byte(buf, (unsigned char)s[i]);
		for (j=0; j<bcs_len; j++) {
			if (s[i] == bcs[j]) {
				M_buf_truncate(buf, M_buf_len(buf)-1);
				M_buf_add_str(buf, a);
				break;
			}
		}
	}

	return M_buf_finish_str(buf, NULL);
}

char *M_strdup_replace_str(const char *s, const char *b, const char *a)
{
	M_buf_t       *buf;
	const char    *p;
	const char    *lp;
	size_t         s_len;
	size_t         b_len;

	if (s == NULL)
		return NULL;
	if (*s == '\0')
		return M_strdup("");
	if (b == NULL || *b == '\0')
		return M_strdup(s);

	s_len = M_str_len(s);
	b_len = M_str_len(b);
	buf   = M_buf_create();

	p  = s;
	lp = p;
	while (1) {
		p = M_str_str(p, b);
		if (p == NULL) {
			break;
		}

		M_buf_add_bytes(buf, (const unsigned char *)lp, (size_t)(p-lp));
		M_buf_add_str(buf, a);

		p += b_len;
		if ((size_t)(p-s) >= s_len) {
			break;
		}

		lp = p;
	}

	if ((size_t)(lp-s) <= s_len)
		M_buf_add_str(buf, lp);

	return M_buf_finish_str(buf, NULL);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Fill
 */

/* Like strncpy except takes full buffer size and guarantees a NULL
 * termination */
M_bool M_str_cpy(char *dest, size_t dest_len, const char *src)
{
	return M_str_cpy_max(dest, dest_len, src, SIZE_MAX);
}

M_bool M_str_cpy_max(char *dest, size_t dest_len, const char *src, size_t src_len)
{
	size_t copy_len;

	if (dest == NULL || dest_len == 0)
		return M_FALSE;

	if (src_len == SIZE_MAX)
		src_len = M_str_len(src);

	copy_len = M_MIN(src_len, dest_len-1);

	if (src != NULL)
		M_mem_move(dest, src, copy_len);

	dest[copy_len] = '\0';
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_str_justify(char *dest, size_t destlen, const char *src, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen)
{
	return M_str_justify_max(dest, destlen, src, M_str_len(src), justtype, justchar, justlen);
}

size_t M_str_justify_max(char *dest, size_t destlen, const char *src, size_t srclen, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen)
{
	const char *inptr  = NULL;
	size_t      llen   = 0; /* Left fill len */
	size_t      rlen   = 0; /* Right fill len */

	/* Invalid use */
	if (justlen >= destlen)
		return 0;

	/* See if truncation is disabled, if so, and src exceeds the length, return 0 */
	if ((justtype == M_STR_JUSTIFY_LEFT_NOTRUNC || justtype == M_STR_JUSTIFY_RIGHT_NOTRUNC
		|| justtype == M_STR_JUSTIFY_CENTER_NO_TRUNC) && srclen > justlen)
		return 0;

	/* If src is NULL, srclen is really zero, always override */
	if (src == NULL)
		srclen = 0;

	/* Check for misuse. */
	if ((unsigned int)justtype >= M_STR_JUSTIFY_END) {
		return 0;
	}

	/* Figure out truncation */
	if (srclen > justlen) {
		switch (justtype) {
			case M_STR_JUSTIFY_RIGHT_TRUNC_RIGHT:
			case M_STR_JUSTIFY_LEFT_TRUNC_RIGHT:
			case M_STR_JUSTIFY_CENTER_TRUNC_RIGHT:
			case M_STR_JUSTIFY_TRUNC_RIGHT:
				inptr  = src;
				srclen = justlen;
				break;
			case M_STR_JUSTIFY_RIGHT:
			case M_STR_JUSTIFY_LEFT:
			case M_STR_JUSTIFY_CENTER:
			case M_STR_JUSTIFY_TRUNC_LEFT:
				inptr  = src + (srclen - justlen);
				srclen = justlen;
				break;
			default:
				inptr  = src;
				srclen = justlen;
				break;
		}
	} else {
		inptr = src;
		switch (justtype) {
			case M_STR_JUSTIFY_TRUNC_RIGHT:
			case M_STR_JUSTIFY_TRUNC_LEFT:
				/* If we're only truncating (and thus not justifying at all), set the actual
				 * justification length to the same as the source length */
				justlen = srclen;
			default:
				break;
		}
	}

	/* Justify */
	switch(justtype) {
		case M_STR_JUSTIFY_RIGHT:
		case M_STR_JUSTIFY_RIGHT_TRUNC_RIGHT:
		case M_STR_JUSTIFY_RIGHT_NOTRUNC:
			llen = justlen - srclen;
			break;
		case M_STR_JUSTIFY_LEFT:
		case M_STR_JUSTIFY_LEFT_TRUNC_RIGHT:
		case M_STR_JUSTIFY_LEFT_NOTRUNC:
		case M_STR_JUSTIFY_TRUNC_LEFT:
		case M_STR_JUSTIFY_TRUNC_RIGHT:
			rlen = justlen - srclen;
			break;
		case M_STR_JUSTIFY_CENTER:
		case M_STR_JUSTIFY_CENTER_TRUNC_RIGHT:
		case M_STR_JUSTIFY_CENTER_NO_TRUNC:
			llen = (justlen - srclen + 1) / 2;
			rlen = (justlen - srclen) / 2;
			break;
		case M_STR_JUSTIFY_END:
			/* This is a dummy entry which will never be hit */
			break;
	}

	if (srclen)
		M_mem_move(dest + llen, inptr, srclen);

	/* Fill with justification character */
	if (llen)
		M_mem_set(dest, justchar, llen);
	if (rlen)
		M_mem_set(dest+(llen+srclen), justchar, rlen);

	/* NULL terminate */
	dest[justlen] = '\0';

	return justlen;
}

M_END_IGNORE_DEPRECATIONS
