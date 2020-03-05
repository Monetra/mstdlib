/* The MIT License (MIT)
 * 
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

#include <mstdlib/mstdlib_text.h>
#include "email/m_email_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t M_email_process_address_list(const char *group_name, char * const *addresses, size_t num_addresses,
		M_email_error_t (*address_func)(const char *group, const char *name, const char *address, void *thunk),
		void *thunk)
{
	char            **parts     = NULL;
	char             *name      = NULL;
	char             *temp      = NULL;
	char             *address   = NULL;
	size_t            num_parts = 0;
	size_t            part_idx  = 0;
	size_t            len;
	size_t            i;
	M_email_error_t   res       = M_EMAIL_ERROR_SUCCESS;

	if (addresses == NULL || num_addresses == 0)
		return M_EMAIL_ERROR_SUCCESS;

	for (i=0; i<num_addresses; i++) {
		/* Try to split on the start of the email segment if we have the "name <email>" form.
		 * If we don't have that form (it's just an email) parts[0] will be the input. */
		parts = M_str_explode_str_quoted('<', addresses[i], '"', '\\', 2, &num_parts);
		if (parts == NULL || num_parts == 0)
			continue;

		/* More than one part means we have a split so part[0] is the name and part[1] is the email. */
		if (num_parts > 1) {
			temp     = M_strdup_unquote(parts[0], '"', '\\');
			name     = M_strdup_trim(temp);
			M_free(temp);
			temp = NULL;
			part_idx = 1;
		}

		/* Pull out the email. */
		if (part_idx != 0) {
			temp = M_strdup_trim(parts[part_idx]);
			/* If we're not the only thing in the list we
			 * split on <. We need to remove the closing >. */
			len  = M_str_len(temp);
			if (temp[len-1] == '>') {
				temp[len-1] = '\0';
			}
			address = M_strdup_trim(temp);
			M_free(temp);
			temp    = NULL;
		} else {
			address = M_strdup_trim(parts[part_idx]);
		}

		if (!M_verify_email_address(address)) {
			res = M_EMAIL_ERROR_ADDRESS;
			goto done;
		}

		res = address_func(group_name, name, address, thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			goto done;
		}

		M_free(temp);
		M_free(name);
		M_free(address);
		M_str_explode_free(parts, num_parts);
		temp      = NULL;
		name      = NULL;
		address   = NULL;
		parts     = NULL;
		num_parts = 0;
	}

done:
	M_free(temp);
	M_free(name);
	M_free(address);
	M_str_explode_free(parts, num_parts);

	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_error_t M_email_process_address(const char *val, M_email_error_t (*address_func)(const char *group, const char *name, const char *address, void *thunk), void *thunk)
{
	char            **groups        = NULL;
	char            **group         = NULL;
	char            **addresses     = NULL;
	char             *group_name    = NULL;
	char             *temp          = NULL;
	size_t            num_groups    = 0;
	size_t            num_group     = 0;
	size_t            num_addresses = 0;
	size_t            i;
	M_email_error_t   res           = M_EMAIL_ERROR_SUCCESS;

	/* Address entires contain several types of entries.
 	 *
	 * - Single address
	 * - list of addresses comma (,) separated
	 *   - some email clients use semi-colon (not part of an RFC) instead of a comma
	 * - Group referencing one or more emails
	 * - List of groups (RFC 6854). This does use a semicolon as a separator
	 * - List of groups and emails not in a group
 	 *
	 * Giving us these possible scenarios:
	 * - address
	 * - address_list
 	 * - group: adress
 	 * - group: adress_list
	 * - group_list
	 *
	 * An address can be a name and address or just an address. The name can be quoted.
	 * - name <address>
	 * - <address>
	 * - address
	 *
	 * We're going to split on group_lists using ';'. Then, split on ':' (separator
	 * between group name and addresses. Then split on ',' to split the addresses.
	 * Finally, we can parse the address.
	 *
	 * Spitting on ';', then ',' will allow us to support both proper (,) and incorrect
	 * (;) separators.
	 *
	 * A lot of this can be quoted and there can be a lot of white space around
	 * each parts so we're going to do a lot of unquoting and trimming.
 	 */

	/* Split on semicolon. This gives us either a group, email, or an email list in each part. */
	groups = M_str_explode_str_quoted(';', val, '"', '\\', 0, &num_groups);
	if (groups == NULL || num_groups == 0)
		goto done;

	for (i=0; i<num_groups; i++) {
		size_t address_idx = 0;

		/* Split on colon to split off the group name from the addresses. */
		group = M_str_explode_str_quoted(':', groups[i], '"', '\\', 2, &num_group);
		if (group == NULL || num_group == 0) {
			continue;
		}

		/* This might not be group. If we have 2 parts then we do have
 		 * a group and the first part is the name. Otherwise it's an email or email list. */
		if (num_group > 1) {
			temp        = M_strdup_unquote(group[0], '"', '\\');
			group_name  = M_strdup_trim(temp);
			M_free(temp);
			temp = NULL;
			address_idx = 1;
		}

		/* Split address within the group or address list. */
		addresses = M_str_explode_str_quoted(',', group[address_idx], '"', '\\', 0, &num_addresses);
		if (addresses == NULL || num_addresses == 0) {
			/* Groups don't have to have addresses. */
			if (group_name != NULL) {
				res = address_func(group_name, NULL, NULL, thunk);
				M_free(group_name);
				if (res != M_EMAIL_ERROR_SUCCESS) {
					goto done;
				}
			}
			/* Ignore empty elements. */
			continue;
		}

		/* At this point we should have a list with individual addresses. */
		res = M_email_process_address_list(group_name, addresses, num_addresses, address_func, thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			goto done;
		}

		M_free(temp);
		M_free(group_name);
		M_str_explode_free(addresses, num_addresses);
		temp          = NULL;
		group_name    = NULL;
		addresses     = NULL;
		num_addresses = 0;
	}

done:
	M_free(temp);
	M_free(group_name);
	M_str_explode_free(addresses, num_addresses);

	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
