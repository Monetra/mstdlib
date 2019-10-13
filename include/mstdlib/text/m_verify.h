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


#ifndef M_EMAIL_VERIFY_H
#define M_EMAIL_VERIFY_H


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_verify Verification
 *  \ingroup m_text
 *
 * \brief Validation of strings against various standards (email address, domain name, etc.).
 * 
 * @{
 */

typedef enum {
	M_VERIFY_EMAIL_LISTDELIM_AUTO = 0,
	M_VERIFY_EMAIL_LISTDELIM_SEMICOLON,
	M_VERIFY_EMAIL_LISTDELIM_COMMA,
} M_verify_email_listdelim_t;


/*! Verify that the given single email address is valid.
 * 
 * An email address is composed of the following parts:
 * \verbatim display_name<local_part@domain>\endverbatim
 * 
 * The display name may contain double-quotes. Within double-quotes, the full range of symbols may be used (dot,
 * comma, semicolon, at, escaped double-quote, etc.). The space and tab characters are allowed in the display name,
 * even outside of quotes.
 * 
 * If the display name is not provided, the angle brackets around the rest of the email address are optional.
 * 
 * This email address validator differs from the RFC standards for email in the following ways:
 * 
 * \li Double quotes are not allowed, except in the display name (a.k.a. friendly name).
 * \li Comments (chunks of text enclosed by parentheses) are not allowed, though parenthesis are allowed in the
 *     display name if they're surrounded by double-quotes.
 * \li Consecutive dots are allowed in the local part of the address, since they are accepted by gmail.
 * \li Only the following non-alphanumeric characters are allowed in the local part: \verbatim '*+-=^_{}.~ \endverbatim
 * \li IP address domains are not allowed (e.g., joe.smith\@[192.168.2.1] is invalid).
 * \li UTF-8 is not allowed in any part of the address (display name, local part, or domain name).
 * 
 * Example of valid address:
 * \verbatim "Public, John Q. (Support)" <jq..public+support@generic-server.com> \endverbatim
 * 
 * \see M_verify_email_address_list
 * 
 * \param[in] addr  email address to verify
 * \return          TRUE if the address is valid
 */
M_API M_bool M_verify_email_address(const char *addr);


/*! Verify that the given domain name is valid.
 * 
 * Valid domain names are composed of letters, digits, and hyphens ('-'), separated into sections
 * by dots ('.'). Each section is limited to 63 chars, and may not begin or end with a hyphen.
 * The entire domain name is limited to 253 characters - note that a 253 character domain name
 * takes 255 bytes to store with the length-prefixed encoding used by DNS.
 * 
 * \param[in] dname  domain name to verify
 * \return           TRUE if the domain name is valid
 */
M_API M_bool M_verify_domain(const char *dname);


/*! Verify that the given list of email addresses is valid.
 * 
 * The list may be delimited by either commas (',') or semicolons (';'), but not both.
 * This function limits the number of addresses in the list to 100 - this isn't a true
 * standard, but it's the limit used virutally everywhere in practice.
 * 
 * The list parser is tolerant of whitespace - individual addresses are trimmed before they're
 * passed to the email address validator.
 * 
 * The individual addresses in the list may include display names (a.k.a friendly names).
 * 
 * \see M_verify_email_address
 * 
 * \param[in] addr_list  list of email addresses to verify
 * \param[in] delim_type Type of delimiter to expect. Can be auto detected.
 * \return               TRUE if the list and all emails in it are valid
 */
M_API M_bool M_verify_email_address_list(const char *addr_list, M_verify_email_listdelim_t delim_type);

/*! @} */

__END_DECLS

#endif /* M_EMAIL_VERIFY_H */
