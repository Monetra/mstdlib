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
#include <mstdlib/mstdlib_text.h>


#define MAX_LEN_EMAIL        254 /* Max. number of chars in an entire email address */

#define MAX_LEN_EMAIL_LOCAL  64  /* Max. number of chars in local part of email (the part before the '@'). */

#define MAX_LEN_DOMAIN       253 /* Max. number of chars in a domain name (255 bytes, but 253 chars due to encoding)*/

#define MAX_LEN_DNS_LABEL    63  /* Max. number of chars between dots in a domain name. */

#define MAX_EMAIL_RECIPIENTS 100 /* Max. number of recipients for a single email (not standard, but pretty universal) */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const char *allowed_in_local =
    "0123456789"                 /*[0-9]*/
    "abcdefghijklmnopqrstuvwxyz" /*[a-z]*/
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /*[A-Z]*/
    ".'*+-=^_{}~";               /* Symbols list more restrictive than standard, to avoid common conflicts
                                  * with symbols used in shell scripts andweird protocols that have been
                                  * grafted on top of email over the years.
                                  */

static const char *allowed_in_dns_label =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "-";

static const char *allowed_in_display_name =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!#$%&'*+-/=?^_`{|}~ \t";

static const char *allowed_in_quoted_display_name =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!#$%&'*+-/=?^_`{|}~ \t"
    "()<>[]:;@,.";               /* these are only allowed if inside quotes */


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

    if (str == NULL || str_pos == NULL || str == str_pos)
        return M_FALSE;

    /* Count the number of escapes before the current character. */
    escape_count = 0;
    while (str_pos > str && *(str_pos - 1) == escape) {
        escape_count++;
        str_pos--;
    }

    /* If the current char is preceeded by an odd number of escapes, this character is escaped. */
    return (escape_count % 2 == 1)? M_TRUE : M_FALSE;
}


/* Count the number of times 'ch' appears in 'str', not including instances
 * where 'ch' is inside quotes.
 */
static size_t str_count_chars_quoted(const char *str, char ch, char quote, char escape)
{
    const char *str_pos   = str;
    size_t      count     = 0;
    M_bool      in_quotes = M_FALSE;

    if (str == NULL) {
        return 0;
    }

    while (*str_pos != '\0') {
        if (*str_pos == quote && !is_escaped(str, str_pos, escape)) {
            in_quotes = !in_quotes; /* toggle */
        } else if (*str_pos == ch && !in_quotes) {
            count++;
        }
        str_pos++;
    }

    return count;
}


/* Possible formats (all valid):
 * (1) user@example.com
 * (2) <user@example.com>
 * (3) John User <user@example.com>
 * (4) "John Q. User" <user@example.com>
 * (5) "<noise> (junk) []:;@..." <user@example.com>
 */
static M_bool verify_display_name(const char *disp)
{
    M_bool  ret            = M_TRUE;
    char   *quoted_disp;
    char   *unquoted_disp;


    /* Divide content between quoted and unquoted parts. */
    quoted_disp   = M_str_keep_quoted(disp, '"', '\\');
    unquoted_disp = M_str_remove_quoted(disp, '"', '\\');

    if (!M_str_isempty(quoted_disp) && !M_str_ischarset(quoted_disp, allowed_in_quoted_display_name)) {
        ret = M_FALSE;
    }

    if (!M_str_isempty(unquoted_disp) && !M_str_ischarset(unquoted_disp, allowed_in_display_name)) {
        ret = M_FALSE;
    }

    M_free(quoted_disp);
    M_free(unquoted_disp);

    return ret;
}


static M_bool verify_local_part(const char *local_part)
{
    size_t len = M_str_len(local_part);

    if (len == 0 || len > MAX_LEN_EMAIL_LOCAL) {
        return M_FALSE;
    }

    /* Can't start or end with a dot. */
    if (local_part[0] == '.' || local_part[len - 1] == '.') {
        return M_FALSE;
    }

    /* Can't start with a hyphen. */
    if (local_part[0] == '-') {
        return M_FALSE;
    }

    /* Only allowed to contain characters from "allowed_in_local". */
    return M_str_ischarset(local_part, allowed_in_local);
}


static M_bool verify_dns_label(const char *dns_label)
{
    size_t len = M_str_len(dns_label);

    if (len == 0 || len > MAX_LEN_DNS_LABEL) {
        return M_FALSE;
    }

    /* Can't start or end with a hyphen. */
    if (dns_label[0] == '-' || dns_label[len - 1] == '-') {
        return M_FALSE;
    }

    /* Only allowed to contain characters from "allowed_in_dns_label". */
    return M_str_ischarset(dns_label, allowed_in_dns_label);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_verify_domain(const char *dname)
{
    size_t   num_dns_labels = 0;
    char   **dns_labels;
    size_t   i;

    if (M_str_isempty(dname) || M_str_len(dname) > MAX_LEN_DOMAIN) {
        return M_FALSE;
    }

    /* Don't allow comments (in parenthesis) or IP addresses (in brackets). */

    /* Split by dot, keep empty list elements. */
    dns_labels = M_str_explode_str('.', dname, &num_dns_labels);

    for (i=0; i<num_dns_labels; i++) {
        if (!verify_dns_label(dns_labels[i])) {
            M_str_explode_free(dns_labels, num_dns_labels);
            return M_FALSE;
        }
    }

    M_str_explode_free(dns_labels, num_dns_labels);
    return M_TRUE;
}


M_bool M_verify_email_address(const char *addr)
{
    char    *email            = NULL;
    char    *display_name     = NULL;

    size_t   num_email_parts  = 0;
    char   **email_parts      = NULL;

    size_t   len;
    size_t   num_open_angle;
    size_t   num_close_angle;

    M_bool   ret              = M_FALSE;


    len = M_str_len(addr);
    if (len < 3 || len > MAX_LEN_EMAIL) {
        goto done;
    }

    /* Split string between the email address itself, and the display name (if any).
     *
     * Possible formats (all valid):
     * (1) user@example.com
     * (2) <user@example.com>
     * (3) John User <user@example.com>
     * (4) "John Q. User" <user@example.com>
     * (5) "<noise> (junk) []:;@..." <user@example.com>
     */
    num_open_angle  = str_count_chars_quoted(addr, '<', '"', '\\');
    num_close_angle = str_count_chars_quoted(addr, '>', '"', '\\');
    if (num_open_angle != num_close_angle || num_open_angle > 1) {
        /* Make sure there's one pair of unquoted angle brackets, at the most. */
        goto done;
    }
    if (num_open_angle > 0) {
        if(addr[len - 1] != '>') {
            /* If the email address is surrounded by brackets, make sure it's the last thing in the string. */
            goto done;
        }
        email        = M_str_keep_bracketed_quoted(addr, '<', '>', '"', '\\');
        display_name = M_str_remove_bracketed_quoted(addr, '<', '>', '"', '\\');
    } else {
        email        = M_strdup(addr);
    }


    /* Validate display name. */
    if (!M_str_isempty(display_name) && !verify_display_name(display_name)) {
        goto done;
    }


    /* Validate email. */
    /* -- Split by @ sign. Valid addresses must have one, and exactly one, @ sign, since we don't allow quoting. */
    email_parts = M_str_explode_str('@', email, &num_email_parts);
    if (num_email_parts != 2) {
        goto done;
    }
    if (!verify_local_part(email_parts[0])) {
        goto done;
    }
    if (!M_verify_domain(email_parts[1])) {
        goto done;
    }


    /* If we reach here, the address was valid. */
    ret = M_TRUE;

    done:
    M_str_explode_free(email_parts, num_email_parts);
    M_free(display_name);
    M_free(email);
    return ret;
}


M_bool M_verify_email_address_list(const char *addr_list, M_verify_email_listdelim_t delim_type)
{
    size_t          num_addresses  = 0;
    char          **addresses      = NULL;
    size_t          num_commas;
    size_t          num_semicolons;
    unsigned char   delimiter      = ',';
    size_t          i;

    if (M_str_isempty(addr_list)) {
        return M_FALSE;
    }

    /* We do a trim later on that eats all six whitespace chars, but we only want to
     * allow space and tab. So, make sure none of the other four whitespace chars
     * are present in the address list ahead of time.
     */
    if (!M_str_isnotcharset(addr_list, "\r\n\f\v")) {
        return M_FALSE;
    }

    /* List may be delimited either by commas or semicolons (but not both). */
    num_commas     = str_count_chars_quoted(addr_list, ',', '"', '\\');
    num_semicolons = str_count_chars_quoted(addr_list, ';', '"', '\\');
    switch (delim_type) {
        case M_VERIFY_EMAIL_LISTDELIM_AUTO:
            if (num_commas > 0 && num_semicolons > 0) {
                return M_FALSE;
            } else if (num_commas > 0) {
                delimiter = ',';
            } else if (num_semicolons > 0) {
                delimiter = ';';
            }
            break;
        case M_VERIFY_EMAIL_LISTDELIM_COMMA:
            delimiter = ',';
            break;
        case M_VERIFY_EMAIL_LISTDELIM_SEMICOLON:
            delimiter = ';';
            break;
    }

    /* If there are no delimiters, evalute the string as a single address. */
    if ((delimiter == ',' && num_commas == 0) ||
        (delimiter == ';' && num_semicolons == 0))
    {
        return M_verify_email_address(addr_list);
    }

    /* Split by the detected delimiter, the check each individual address (after whitespace trimming).
     * Don't allow empty list entries, fail if we detect one (including entries that are all whitespace).
     */
    addresses = M_str_explode_str_quoted(delimiter, addr_list, '"', '\\', 0, &num_addresses);
    if (num_addresses > MAX_EMAIL_RECIPIENTS) {
        M_str_explode_free(addresses, num_addresses);
        return M_FALSE;
    }
    for (i=0; i<num_addresses; i++) {
        char *address = M_str_trim(addresses[i]);
        if (M_str_isempty(address) || !M_verify_email_address(address)) {
            M_str_explode_free(addresses, num_addresses);
            return M_FALSE;
        }
    }

    M_str_explode_free(addresses, num_addresses);
    return M_TRUE;
}
