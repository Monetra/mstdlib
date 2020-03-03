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


#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_text.h>

#include "email/m_email_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_buf_t       *data;
	M_hash_dict_t *headers;

	M_bool         is_attachment;
	char          *content_type;
	char          *transfer_encoding;
	char          *filename;
} M_email_message_part_t;

typedef struct {
	char *group;
	char *name;
	char *address;
} M_email_message_address_t;

struct M_email_message {
	M_hash_dict_t             *headers;
	M_list_t                  *to;    /* List of M_email_message_address_t */
	M_list_t                  *cc;    /* List of M_email_message_address_t */
	M_list_t                  *bcc;   /* List of M_email_message_address_t */
	M_list_t                  *parts; /* List of M_email_message_part_t */
	char                      *preamble;
	char                      *epilogue;
	M_email_message_address_t *reply_to;
	M_email_message_address_t *from;
	char                      *subject;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int address_list_cmp(const void *a, const void *b)
{
	const M_email_message_address_t *sa = *(const M_email_message_address_t **)a;
	const M_email_message_address_t *sb = *(const M_email_message_address_t **)b;

	return M_str_cmpsort(sa->group, sb->group);
}

static M_email_message_address_t *M_email_message_address_create(const char *group, const char *name, const char *address)
{
	M_email_message_address_t *ad;

	ad          = M_malloc_zero(sizeof(*ad));
	ad->group   = M_strdup(group);
	ad->name    = M_strdup(name);
	ad->address = M_strdup(address);

	return ad;
}

static void M_email_message_address_destroy(M_email_message_address_t *ad)
{
	if (ad == NULL)
		return;

	M_free(ad->group);
	M_free(ad->name);
	M_free(ad->address);

	M_free(ad);
}

static M_list_t *create_address_list(void)
{
	struct M_list_callbacks cbs = {
		(M_sort_compar_t)address_list_cmp,
		NULL,
		NULL,
		(M_list_free_func)M_email_message_address_destroy
	};

	return M_list_create(&cbs, M_LIST_SORTED);
}

static M_bool M_email_message_address_entry(M_email_message_address_t *ad, char const **group, char const **name, char const **address)
{
	if (ad == NULL ||
			(M_str_isempty(ad->group) &&
			 M_str_isempty(ad->name)  &&
			 M_str_isempty(ad->address)))
	{
		if (group != NULL) {
			*group   = NULL;
		}
		if (name != NULL) {
			*name    = NULL;
		}
		if (address != NULL) {
			*address = NULL;
		}
		return M_FALSE;
	}

	if (group != NULL)
		*group   = ad->group;
	if (name != NULL)
		*name    = ad->name;
	if (address != NULL)
		*address = ad->address;

	return M_TRUE;
}

static M_email_error_t set_address_list(const char *group, const char *name, const char *address, void *thunk)
{
	M_email_message_address_t *ad;
	M_list_t                  *alist = thunk;

	ad = M_email_message_address_create(group, name, address);
	M_list_insert(alist, ad);

	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t set_single_address(const char *group, const char *name, const char *address, void *thunk)
{
	M_email_message_address_t *ad = thunk;

	M_free(ad->group);
	M_free(ad->name);
	M_free(ad->address);
	ad->group   = M_strdup(group);
	ad->name    = M_strdup(name);
	ad->address = M_strdup(address);

	return M_EMAIL_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_message_part_t *M_email_message_part_create(void)
{
	M_email_message_part_t *part;

	part          = M_malloc_zero(sizeof(*part));
	part->data    = M_buf_create();
	part->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);

	return part;
}

static void M_email_message_part_destroy(M_email_message_part_t *part)
{
	if (part == NULL)
		return;

	M_buf_cancel(part->data);
	M_free(part->content_type);
	M_free(part->transfer_encoding);
	M_free(part->filename);
	M_hash_dict_destroy(part->headers);

	M_free(part);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_t *create_part_list(void)
{
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_email_message_part_destroy
	};
	return M_list_create(&cbs, M_LIST_NONE);
}

static M_bool append_part_is_attachment(const M_hash_dict_t *headers)
{
	const char *cd;

	cd = M_hash_dict_get_direct(headers, "Content-Disposition");
	if (M_str_isempty(cd))
		return M_FALSE;

	if (M_str_casestr(cd, "attachment"))
		return M_TRUE;
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_message_t *M_email_message_create(void)
{
	M_email_message_t *message;

	message           = M_malloc_zero(sizeof(*message));
	message->headers  = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);

	message->to       = create_address_list();
	message->cc       = create_address_list();
	message->bcc      = create_address_list();
	message->reply_to = M_email_message_address_create(NULL, NULL, NULL);
	message->from     = M_email_message_address_create(NULL, NULL, NULL);

	message->parts    = create_part_list();

	return message;
}

void M_email_message_destroy(M_email_message_t *message)
{
	if (message == NULL)
		return;

	M_hash_dict_destroy(message->headers);
	M_list_destroy(message->to, M_TRUE);
	M_list_destroy(message->cc, M_TRUE);
	M_list_destroy(message->bcc, M_TRUE);
	M_email_message_address_destroy(message->reply_to);
	M_email_message_address_destroy(message->from);
	M_list_destroy(message->parts, M_TRUE);
	M_free(message->preamble);
	M_free(message->epilogue);
	M_free(message->subject);

	M_free(message);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_message_set_headers(M_email_message_t *message, const M_hash_dict_t *headers)
{
	M_hash_dict_t             *new_headers;
	M_list_t                  *to       = NULL;
	M_list_t                  *cc       = NULL;
	M_list_t                  *bcc      = NULL;
	M_email_message_address_t *reply_to = NULL;
	M_email_message_address_t *from     = NULL;
	char                      *subject  = NULL;
	M_hash_dict_enum_t        *he;
	const char                *key;
	const char                *val;
	M_email_error_t            res      = M_EMAIL_ERROR_SUCCESS;

	if (message == NULL)
		return M_FALSE;

	new_headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);
	to          = create_address_list();
	cc          = create_address_list();
	bcc         = create_address_list();
	reply_to    = M_email_message_address_create(NULL, NULL, NULL);
	from        = M_email_message_address_create(NULL, NULL, NULL);

	/* Enumerating and not duplicating because we want to ensure we have
 	 * a single value dict with casecmp keys. */
	M_hash_dict_enumerate(headers, &he);
	while (res == M_EMAIL_ERROR_SUCCESS && M_hash_dict_enumerate_next(headers, he, &key, &val)) {
		if (M_str_caseeq(key, "To")) {
			res = M_email_process_address(val, set_address_list, to);
		} else if (M_str_caseeq(key, "CC")) {
			res = M_email_process_address(val, set_address_list, cc);
		} else if (M_str_caseeq(key, "BCC")) {
			res = M_email_process_address(val, set_address_list, bcc);
		} else if (M_str_caseeq(key, "Reply-To")) {
			res = M_email_process_address(val, set_single_address, reply_to);
		} else if (M_str_caseeq(key, "From")) {
			res = M_email_process_address(val, set_single_address, from);
		} else if (M_str_caseeq(key, "Subject")) {
			subject = M_strdup(val);
		} else {
			M_hash_dict_insert(new_headers, key, val);
		}
	}
	M_hash_dict_enumerate_free(he);

	if (res != M_EMAIL_ERROR_SUCCESS) {
		M_list_destroy(to, M_TRUE);
		M_list_destroy(cc, M_TRUE);
		M_list_destroy(bcc, M_TRUE);
		M_email_message_address_destroy(reply_to);
		M_email_message_address_destroy(from);
		M_free(subject);
		M_hash_dict_destroy(new_headers);
		return M_FALSE;
	}

	M_hash_dict_destroy(message->headers);
	message->headers = new_headers;

	M_list_destroy(message->to, M_TRUE);
	message->to = to;

	M_list_destroy(message->cc, M_TRUE);
	message->cc = cc;

	M_list_destroy(message->bcc, M_TRUE);
	message->bcc = bcc;

	M_email_message_address_destroy(message->reply_to);
	message->reply_to = reply_to;

	M_email_message_address_destroy(message->from);
	message->from = from;

	M_free(message->subject);
	message->subject = subject;

	return M_TRUE;
}

M_bool M_email_message_headers_insert(M_email_message_t *message, const char *key, const char *val)
{
	M_list_t                  *alist = NULL;
	M_email_message_address_t *ad    = NULL;
	M_email_error_t            res;

	if (message == NULL || M_str_isempty(key))
		return M_FALSE;

	if (M_str_caseeq(key, "To") || M_str_caseeq(key, "CC") || M_str_caseeq(key, "BCC")) {
		alist = create_address_list();
		res   = M_email_process_address(val, set_address_list, alist);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			M_list_destroy(alist, M_TRUE);
			return M_FALSE;
		}

		if (M_str_caseeq(key, "To")) {
			M_list_destroy(message->to, M_TRUE);
			message->to = alist;
		} else if (M_str_caseeq(key, "CC")) {
			M_list_destroy(message->cc, M_TRUE);
			message->cc = alist;
		} else if (M_str_caseeq(key, "BCC")) {
			M_list_destroy(message->bcc, M_TRUE);
			message->bcc = alist;
		}

		return M_TRUE;
	}

	if (M_str_caseeq(key, "Reply-To") || M_str_caseeq(key, "From")) {
		ad  = M_email_message_address_create(NULL, NULL, NULL);
		res = M_email_process_address(val, set_single_address, ad);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			M_email_message_address_destroy(ad);
			return M_FALSE;
		}

		if (M_str_caseeq(key, "Reply-To")) {
			M_email_message_address_destroy(message->reply_to);
			message->reply_to = ad;
		} else if (M_str_caseeq(key, "From")) {
			M_email_message_address_destroy(message->from);
			message->from = ad;
		}

		return M_TRUE;
	}

	if (M_str_caseeq(key, "Subject")) {
		M_free(message->subject);
		message->subject = M_strdup(val);
		return M_TRUE;
	}

	return M_hash_dict_insert(message->headers, key, val);
}

void M_email_message_headers_remove(M_email_message_t *message, const char *key)
{
	if (message == NULL || M_str_isempty(key))
		return;

	M_hash_dict_remove(message->headers, key);
}

const M_hash_dict_t *M_email_message_headers(const M_email_message_t *message)
{
	if (message == NULL)
		return NULL;

	return message->headers;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_message_from(const M_email_message_t *message, char const **group, char const **name, char const **address)
{
	if (message == NULL)
		return M_FALSE;
	return M_email_message_address_entry(message->from, group, name, address);
}

void M_email_message_set_from(M_email_message_t *message, const char *group, const char *name, const char *address)
{
	M_email_message_address_t *ad;

	if (message == NULL)
		return;

	/* Set before destroy in case the input parameters are
 	 * from M_email_message_from because something is being added
	 * to the entry. */
	ad = M_email_message_address_create(group, name, address);
	M_email_message_address_destroy(message->from);
	message->from = ad;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_message_to_len(const M_email_message_t *message)
{
	if (message == NULL)
		return 0;

	return M_list_len(message->to);
}

M_bool M_email_message_to(const M_email_message_t *message, size_t idx, char const **group, char const **name, char const **address)
{
	if (message == NULL)
		return M_FALSE;
	return M_email_message_address_entry((M_email_message_address_t *)M_list_at(message->to, idx), group, name, address);
}

void M_email_message_to_append(M_email_message_t *message, const char *group, const char *name, const char *address)
{
	if (message == NULL)
		return;
	M_list_insert(message->to, M_email_message_address_create(group, name, address));
}

void M_email_message_to_remove(M_email_message_t *message, size_t idx)
{
	if (message == NULL)
		return;
	M_list_remove_at(message->to, idx);
}

void M_email_message_to_clear(M_email_message_t *message)
{
	if (message == NULL)
		return;
	M_list_destroy(message->to, M_TRUE);
	message->to = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_message_cc_len(const M_email_message_t *message)
{
	if (message == NULL)
		return 0;

	return M_list_len(message->cc);
}

M_bool M_email_message_cc(const M_email_message_t *message, size_t idx, char const **group, char const **name, char const **address)
{
	if (message == NULL)
		return M_FALSE;
	return M_email_message_address_entry((M_email_message_address_t *)M_list_at(message->cc, idx), group, name, address);
}

void M_email_message_cc_append(M_email_message_t *message, const char *group, const char *name, const char *address)
{
	if (message == NULL)
		return;
	M_list_insert(message->cc, M_email_message_address_create(group, name, address));
}

void M_email_message_cc_remove(M_email_message_t *message, size_t idx)
{
	if (message == NULL)
		return;
	M_list_remove_at(message->cc, idx);
}

void M_email_message_cc_clear(M_email_message_t *message)
{
	if (message == NULL)
		return;
	M_list_destroy(message->cc, M_TRUE);
	message->cc = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_message_bcc_len(const M_email_message_t *message)
{
	if (message == NULL)
		return 0;

	return M_list_len(message->bcc);
}

M_bool M_email_message_bcc(const M_email_message_t *message, size_t idx, char const **group, char const **name, char const **address)
{
	if (message == NULL)
		return M_FALSE;
	return M_email_message_address_entry((M_email_message_address_t *)M_list_at(message->bcc, idx), group, name, address);
}

void M_email_message_bcc_append(M_email_message_t *message, const char *group, const char *name, const char *address)
{
	if (message == NULL)
		return;
	M_list_insert(message->bcc, M_email_message_address_create(group, name, address));
}

void M_email_message_bcc_remove(M_email_message_t *message, size_t idx)
{
	if (message == NULL)
		return;
	M_list_remove_at(message->bcc, idx);
}

void M_email_message_bcc_clear(M_email_message_t *message)
{
	if (message == NULL)
		return;
	M_list_destroy(message->bcc, M_TRUE);
	message->bcc = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_message_reply_to(const M_email_message_t *message, char const **group, char const **name, char const **address)
{
	if (message == NULL)
		return M_FALSE;
	return M_email_message_address_entry(message->reply_to, group, name, address);
}

void M_email_message_set_reply_to(M_email_message_t *message, const char *group, const char *name, const char *address)
{
	M_email_message_address_t *ad;

	if (message == NULL)
		return;

	/* Set before destroy in case the input parameters are
 	 * reply_to M_email_message_reply_to because something is being added
	 * to the entry. */
	ad = M_email_message_address_create(group, name, address);
	M_email_message_address_destroy(message->reply_to);
	message->reply_to = ad;
}

void M_email_message_reply_to_remove(M_email_message_t *message)
{
	if (message == NULL)
		return;

	M_email_message_address_destroy(message->reply_to);
	message->reply_to = M_email_message_address_create(NULL, NULL, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_email_message_set_subject(M_email_message_t *message, const char *subject)
{
	if (message == NULL)
		return;

	M_free(message->subject);
	message->subject = M_strdup(subject);
}

const char *M_email_message_subject(const M_email_message_t *message)
{
	if (message == NULL)
		return NULL;
	return message->subject;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_email_message_preamble(M_email_message_t *message)
{
	if (message == NULL)
		return NULL;
	return message->preamble;
}

void M_email_message_set_preamble(M_email_message_t *message, const char *data)
{
	if (message == NULL)
		return;

	M_free(message->preamble);
	message->preamble = M_strdup(data);
}

const char *M_email_message_epilouge(M_email_message_t *message)
{
	if (message == NULL)
		return NULL;
	return message->epilogue;
}

void M_email_message_set_epilouge(M_email_message_t *message, const char *data)
{
	if (message == NULL)
		return;

	M_free(message->epilogue);
	message->epilogue = M_strdup(data);
}

M_bool M_email_message_part_append(M_email_message_t *message, const char *data, M_hash_dict_t *headers, size_t *idx)
{
	M_email_message_part_t *part;
	M_hash_dict_enum_t     *he;
	const char             *key;
	const char             *val;
	size_t                  myidx;

	if (idx == NULL)
		idx = &myidx;
	*idx = 0;

	if (message == NULL)
		return M_FALSE;

	if (append_part_is_attachment(headers))
		return M_email_message_part_append_attachment(message, data, headers, NULL, NULL, NULL, idx);

	part = M_email_message_part_create();
	M_buf_add_str(part->data, data);

	if (headers != NULL) {
		M_hash_dict_enumerate(headers, &he);
		while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
			M_hash_dict_insert(part->headers, key, val);
		}
		M_hash_dict_enumerate_free(he);
	}

	if (!M_list_insert(message->parts, part)) {
		M_email_message_part_destroy(part);
		return M_FALSE;
	}

	*idx = M_list_len(message->parts)-1;
	return M_TRUE;
}

M_bool M_email_message_part_append_attachment(M_email_message_t *message, const char *data, M_hash_dict_t *headers, const char *content_type, const char *transfer_encoding, const char *filename, size_t *idx)
{
	M_email_message_part_t *part;
	M_hash_dict_enum_t     *he;
	const char             *key;
	const char             *val;
	size_t                  myidx;

	if (idx == NULL)
		idx = &myidx;
	*idx = 0;

	if (message == NULL)
		return M_FALSE;

	part                    = M_email_message_part_create();
	M_buf_add_str(part->data, data);
	part->content_type      = M_strdup(content_type);
	part->transfer_encoding = M_strdup(transfer_encoding);
	part->filename          = M_strdup(filename);
	part->is_attachment     = M_TRUE;

	if (headers != NULL) {
		M_hash_dict_enumerate(headers, &he);
		while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
			if (M_str_caseeq(key, "Content-Type") || 
					M_str_caseeq(key, "Content-Disposition") ||
					M_str_caseeq(key, "Content-Transfer-Encoding"))
			{
				continue;
			}
			M_hash_dict_insert(part->headers, key, val);
		}
		M_hash_dict_enumerate_free(he);
	}

	if (!M_list_insert(message->parts, part)) {
		M_email_message_part_destroy(part);
		return M_FALSE;
	}

	*idx = M_list_len(message->parts)-1;
	return M_TRUE;
}

M_bool M_email_message_part_append_data(M_email_message_t *message, size_t idx, const char *data, size_t len)
{
	M_email_message_part_t *part;

	if (message == NULL)
		return M_FALSE;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return M_FALSE;

	M_buf_add_bytes(part->data, data, len);
	return M_TRUE;
}

M_bool M_email_message_part_set_data(M_email_message_t *message, size_t idx, const char *data)
{
	M_email_message_part_t *part;

	if (message == NULL)
		return M_FALSE;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return M_FALSE;

	M_buf_truncate(part->data, 0);
	M_buf_add_str(part->data, data);

	return M_TRUE;
}

size_t M_email_message_parts_len(const M_email_message_t *message)
{
	if (message == NULL)
		return 0;
	return M_list_len(message->parts);
}

void M_email_message_parts_clear(M_email_message_t *message)
{
	if (message == NULL)
		return;
	M_list_destroy(message->parts, M_TRUE);
	message->parts = create_part_list();
}

const char *M_email_message_part_data(const M_email_message_t *message, size_t idx)
{
	M_email_message_part_t *part;

	if (message == NULL)
		return NULL;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return NULL;
	return M_buf_peek(part->data);
}

const M_hash_dict_t *M_email_message_part_headers(const M_email_message_t *message, size_t idx)
{
	M_email_message_part_t *part;

	if (message == NULL)
		return NULL;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return NULL;
	return part->headers;
}

M_bool M_email_message_part_is_attachmenet(const M_email_message_t *message, size_t idx)
{
	M_email_message_part_t *part;

	if (message == NULL)
		return M_FALSE;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return M_FALSE;
	return part->is_attachment;
}

M_bool M_email_message_part_attachment_info(const M_email_message_t *message, size_t idx, char const **content_type, char const **transfer_encoding, char const **filename)
{
	M_email_message_part_t *part;

	if (content_type != NULL)
		*content_type = NULL;
	if (transfer_encoding == NULL)
		*transfer_encoding = NULL;
	if (filename != NULL)
		filename = NULL;

	if (message == NULL)
		return M_FALSE;

	part = (M_email_message_part_t *)M_list_at(message->parts, idx);
	if (part == NULL)
		return M_FALSE;

	if (content_type != NULL)
		*content_type = part->content_type;
	if (transfer_encoding == NULL)
		*transfer_encoding = part->transfer_encoding;
	if (filename != NULL)
		*filename = part->filename;

	return M_TRUE;
}

void M_email_message_part_remove(M_email_message_t *message, size_t idx)
{
	if (message == NULL)
		return;

	M_list_remove_at(message->parts, idx);
}
