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
			M_str_explode_free(group, num_group);
			group     = NULL;
			num_group = 0;
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
		M_str_explode_free(group, num_group);
		temp          = NULL;
		group_name    = NULL;
		addresses     = NULL;
		num_addresses = 0;
		group         = NULL;
		num_group     = 0;
	}

done:
	M_free(temp);
	M_free(group_name);
	M_str_explode_free(addresses, num_addresses);
	M_str_explode_free(group, num_group);
	M_str_explode_free(groups, num_groups);

	return res;
}

static char *M_email_address_format(const char *name, const char *address)
{
	M_buf_t *buf;

	if (M_str_isempty(address))
		return NULL;

	buf = M_buf_create();

	if (M_str_isempty(name)) {
		M_buf_add_str(buf, address);
		return M_buf_finish_str(buf, NULL);
	}

	M_buf_add_str_quoted(buf, '\"', '\\', "<>,@.", M_FALSE, name);
	M_buf_add_byte(buf, ' ');
	M_buf_add_byte(buf, '<');
	M_buf_add_str(buf, address);
	M_buf_add_byte(buf, '>');

	return M_buf_finish_str(buf, NULL);
}

static char *M_email_address_format_group(const char *group, const char *address_list)
{
	M_buf_t *buf;

	if (M_str_isempty(group))
		return M_strdup(address_list);

	buf = M_buf_create();
	M_buf_add_str(buf, group);
	M_buf_add_str(buf, ": ");
	M_buf_add_str(buf, address_list);

	return M_buf_finish_str(buf, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Content-Disposition */
M_bool M_email_attachment_parse_info_attachment(const char *val, char **filename)
{
	M_parser_t  *parser        = NULL;
	M_parser_t **parts         = NULL;
	char        *myfilename    = NULL;
	size_t       num_parts     = 0;
	size_t       i;
	M_bool       is_attachment = M_FALSE;

	parser = M_parser_create_const((const unsigned char *)val, M_str_len(val), M_PARSER_FLAG_NONE);
	parts  = M_parser_split(parser, ';', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);

	for (i=0; i<num_parts; i++) {
		M_parser_consume_whitespace(parts[i], M_PARSER_WHITESPACE_NONE);
		M_parser_truncate_whitespace(parts[i], M_PARSER_WHITESPACE_NONE);

		if (M_parser_compare_str(parts[i], "attachment", 0, M_TRUE)) {
			is_attachment = M_TRUE;
		} else if (M_parser_consume_str_until(parts[i], "filename=", M_TRUE) != 0) {
			M_parser_consume_until(parts[i], (const unsigned char *)"\"", 1, M_TRUE);
			M_parser_truncate_until(parts[i], (const unsigned char *)"\"", 1, M_TRUE);

			M_free(myfilename);
			myfilename = M_parser_read_strdup(parts[i], M_parser_len(parts[i]));
		}
	}

	M_parser_destroy(parser);
	M_parser_split_free(parts, num_parts);

	if (is_attachment) {
		if (filename != NULL) {
			*filename = myfilename;
		} else {
			M_free(myfilename);
		}
		return M_TRUE;
	}

	M_free(myfilename);
	return M_FALSE;
}

/* Content-Type */
char *M_email_attachment_parse_info_content_type(const char *val, char **filename)
{
	M_parser_t    *parser     = NULL;
	M_parser_t   **parts      = NULL;
	M_list_str_t  *abriged    = NULL;
	char          *myfilename = NULL;
	size_t         num_parts  = 0;
	size_t         i;
	char          *out;

	parser  = M_parser_create_const((const unsigned char *)val, M_str_len(val), M_PARSER_FLAG_NONE);
	parts   = M_parser_split(parser, ';', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	abriged = M_list_str_create(M_LIST_STR_NONE);

	for (i=0; i<num_parts; i++) {
			M_parser_consume_whitespace(parts[i], M_PARSER_WHITESPACE_NONE);
			M_parser_truncate_whitespace(parts[i], M_PARSER_WHITESPACE_NONE);

			/* Content-Type: application/octet-stream; name="file.log"
 			 * Content-Type: text/xml; charset=UTF-8; x-mac-type="0"; x-mac-creator="0"; */
			M_parser_mark(parts[i]);
			if (M_str_isempty(myfilename) && M_parser_consume_str_until(parts[i], "name=", M_TRUE) != 0) {
				M_parser_mark_rewind(parts[i]);

				M_parser_consume_until(parts[i], (const unsigned char *)"\"", 1, M_TRUE);
				M_parser_truncate_until(parts[i], (const unsigned char *)"\"", 1, M_TRUE);

				M_free(myfilename);
				myfilename = M_parser_read_strdup(parts[i], M_parser_len(parts[i]));
			} else {
				out = M_parser_read_strdup(parts[i], M_parser_len(parts[i]));
				M_list_str_insert(abriged, out);
				M_free(out);
			}
	}

	M_parser_destroy(parser);
	M_parser_split_free(parts, num_parts);

	out = M_list_str_join_str(abriged, "; ");
	M_list_str_destroy(abriged);
	if (filename != NULL) {
		*filename = myfilename;
	} else {
		M_free(myfilename);
	}

	return out;
}

header_state_t M_email_header_get_next(M_parser_t *parser, char **key, char **val)
{
	M_parser_t      *header = NULL;
	M_buf_t         *buf    = NULL;
	M_parser_t     **kv     = NULL;
	char            *temp   = NULL;
	header_state_t   hsres  = HEADER_STATE_SUCCESS;
	size_t           num_kv = 0;

	*key = NULL;
	*val = NULL;

	/* An empty line means the end of the header. */
	if (M_parser_compare_str(parser, "\r\n", 2, M_FALSE)) {
		M_parser_consume(parser, 2);
		return HEADER_STATE_END;
	}

	/* Mark the header because we need to rewind if we don't have
	 * a full header. Headers can span multiple lines and we want
	 * to parser a complete header not line by line because some
	 * data like an email address in the 'TO' header can be split
	 * on spaces across lines. */
	M_parser_mark(parser);

	/* Use a buf because we need to join lines. */
	buf = M_buf_create();
	while (1) {
		if (M_parser_read_buf_until(parser, buf, (const unsigned char *)"\r\n", 2, M_FALSE) == 0) {
			/* Not enough data so nothing to do. */
			M_parser_mark_rewind(parser);
			hsres = HEADER_STATE_MOREDATA;
			goto done;
		}
		/* Eat the \r\n after the header. */
		M_parser_consume(parser, 2);

		/* If there is nothing after we don't know if we'll have a new header, end of header,
		 * or continuation line. We need to wait for more data. */
		if (M_parser_len(parser) == 0) {
			M_parser_mark_rewind(parser);
			hsres = HEADER_STATE_MOREDATA;
			goto done;
		}

		/* If we have space or tab starting a line then this is a continuation line
		 * for the header. We'll kill the empty space and add a single one to join
		 * this line to the rest of the header. */
		if (M_parser_consume_charset(parser, (const unsigned char *)" \t", 2) != 0) {
			M_buf_add_byte(buf, ' ');
		} else {
			break;
		}
	}
	M_parser_mark_clear(parser);

	/* buf is filled with a full header. */
	header = M_parser_create_const((const unsigned char *)M_buf_peek(buf), M_buf_len(buf), M_PARSER_FLAG_NONE);

	/* Split the key from the value. */
	kv = M_parser_split(header, ':', 2, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &num_kv);
	if (kv == NULL || num_kv == 0) {
		hsres = HEADER_STATE_FAIL;
		goto done;
	}

	/* Spaces between the key and separator (:) are _NOT_allowed. */
	if (M_parser_truncate_whitespace(kv[0], M_PARSER_WHITESPACE_NONE) != 0) {
		hsres = HEADER_STATE_FAIL;
		goto done;
	}

	/* Validate we actually have a key. */
	if (M_parser_len(kv[0]) == 0) {
		hsres = HEADER_STATE_FAIL;
		goto done;
	}

	/* Get the key. */
	temp = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));
	*key = M_strdup_trim(temp);
	M_free(temp);

	/* We support a header being sent without a value. If there is a value
	 * we'll pull it off. */
	if (num_kv == 2) {
		/* Spaces between the separator (:) and value are allowed and should be ignored. Consume them. */
		M_parser_consume_whitespace(kv[1], M_PARSER_WHITESPACE_NONE);
		temp = M_parser_read_strdup(kv[1], M_parser_len(kv[1]));
		*val = M_strdup_trim(temp);
		M_free(temp);
	}

done:
		M_parser_destroy(header);
		M_buf_cancel(buf);
		M_parser_split_free(kv, num_kv);
		return hsres;
}

char *M_email_write_recipients(const M_email_t *email, M_email_recp_len_func_t recp_len, M_email_recp_func_t recp)
{
	M_hash_strvp_t      *group_entires;
	M_list_str_t        *non_group_entires;
	M_list_str_t        *recp_list;
	M_list_str_t        *l;
	M_hash_strvp_enum_t *he;
	const char          *group;
	const char          *name;
	const char          *address;
	char                *out   = NULL;
	char                *full;
	size_t               len;
	size_t               i;

	len = recp_len(email);
	if (len == 0)
		return NULL;

	group_entires     = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void(*)(void *))M_list_str_destroy);
	non_group_entires = M_list_str_create(M_LIST_STR_NONE);

	for (i=0; i<len; i++) {
		group   = NULL;
		name    = NULL;
		address = NULL;

		recp(email, i, &group, &name, &address);
		full = M_email_address_format(name, address);

		if (!M_str_isempty(group)) {
			/* Get the list for this group. Creating a new
 			 * list if there isn't one already. */
			l = M_hash_strvp_get_direct(group_entires, group);
			if (l == NULL) {
				l = M_list_str_create(M_LIST_STR_NONE);
				M_hash_strvp_insert(group_entires, group, l);
			}

			/* We might not have a name/address because it's
 			 * an empty (valid) group. Don't add anything if
			 * this is the case. */
			if (!M_str_isempty(full)) {
				M_list_str_insert(l, full);
			}
		} else if (!M_str_isempty(full)) {
			/* No group, add it to our list of non-grouped entires. */
			M_list_str_insert(non_group_entires, full);
		}

		M_free(full);
	}

	/* Go through all the groups and put together the full
 	 * group lists. */
	recp_list = M_list_str_create(M_LIST_STR_NONE);
	M_hash_strvp_enumerate(group_entires, &he);
	while (M_hash_strvp_enumerate_next(group_entires, he, &group, (void **)&l)) {
		/* Name/addresses are separated by a comma. */
		out  = M_list_str_join_str(l, ", ");
		/* Put the list together with the group. */
		full = M_email_address_format_group(group, out);
		M_free(out);
		M_list_str_insert(recp_list, full);
		M_free(full);
	}
	M_hash_strvp_enumerate_free(he);

	/* Create our list of non-grouped entries and
 	 * append it to our group list (there might not be any groups). */
	out = M_list_str_join_str(non_group_entires, ", ");
	M_list_str_insert(recp_list, out);
	M_free(out);

	/* Groups are separated by a semicolon. */
	out = M_list_str_join_str(recp_list, "; ");

	/* Clean up. */
	M_list_str_destroy(non_group_entires);
	M_list_str_destroy(recp_list);
	M_hash_strvp_destroy(group_entires, M_TRUE);

	return out;
}

char *M_email_write_single_recipient(const char *group, const char *name, const char *address)
{
	char *ad;
	char *full;

	if (M_str_isempty(group) && M_str_isempty(address))
		return NULL;

	ad   = M_email_address_format(name, address);
	full = M_email_address_format_group(group, ad);

	M_free(ad);
	return full;
}
