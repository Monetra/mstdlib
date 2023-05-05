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

#include "email/m_email_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_buf_t       *data;
	M_hash_dict_t *headers;

	M_bool         is_attachment;
	char          *content_type;
	char          *transfer_encoding;
	char          *filename;
} M_email_part_t;

typedef struct {
	char *group;
	char *name;
	char *address;
} M_email_address_t;

struct M_email {
	M_hash_dict_t             *headers;
	M_list_t                  *to;    /* List of M_email_address_t */
	M_list_t                  *cc;    /* List of M_email_address_t */
	M_list_t                  *bcc;   /* List of M_email_address_t */
	M_list_t                  *parts; /* List of M_email_part_t */
	char                      *preamble;
	char                      *epilogue;
	M_email_address_t         *reply_to;
	M_email_address_t         *from;
	char                      *subject;
	M_bool                     is_mixed_multipart;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int address_list_cmp(const void *a, const void *b, void *thunk)
{
	const M_email_address_t *sa = *(const M_email_address_t **)a;
	const M_email_address_t *sb = *(const M_email_address_t **)b;

	(void)thunk;

	return M_str_cmpsort(sa->group, sb->group);
}

static M_email_address_t *M_email_address_create(const char *group, const char *name, const char *address)
{
	M_email_address_t *ad;

	ad          = M_malloc_zero(sizeof(*ad));
	ad->group   = M_strdup(group);
	ad->name    = M_strdup(name);
	ad->address = M_strdup(address);

	return ad;
}

static void M_email_address_destroy(M_email_address_t *ad)
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
		(M_list_free_func)M_email_address_destroy
	};

	return M_list_create(&cbs, M_LIST_SORTED);
}

static M_bool M_email_address_entry(const M_email_address_t *ad, char const **group, char const **name, char const **address)
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
	M_email_address_t *ad;
	M_list_t                  *alist = thunk;

	ad = M_email_address_create(group, name, address);
	M_list_insert(alist, ad);

	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t set_single_address(const char *group, const char *name, const char *address, void *thunk)
{
	M_email_address_t *ad = thunk;

	M_free(ad->group);
	M_free(ad->name);
	M_free(ad->address);
	ad->group   = M_strdup(group);
	ad->name    = M_strdup(name);
	ad->address = M_strdup(address);

	return M_EMAIL_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_part_t *M_email_part_create(void)
{
	M_email_part_t *part;

	part          = M_malloc_zero(sizeof(*part));
	part->data    = M_buf_create();
	part->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);

	return part;
}

static void M_email_part_destroy(M_email_part_t *part)
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
		(M_list_free_func)M_email_part_destroy
	};
	return M_list_create(&cbs, M_LIST_NONE);
}

static M_bool append_part_is_attachment(const M_hash_dict_t *headers)
{
	const char *const_temp;

	const_temp = M_hash_dict_get_direct(headers, "Content-Disposition");
	return M_email_attachment_parse_info_attachment(const_temp, NULL);
}

static M_bool parse_insert_attachment(M_email_t *email, const char *data, size_t len, const M_hash_dict_t *headers, size_t *idx)
{
	const char *const_temp;
	char       *content_type      = NULL;
	char       *filename          = NULL;
	char       *transfer_encoding = NULL;
	M_bool      ret;

	const_temp = M_hash_dict_get_direct(headers, "Content-Transfer-Encoding");
	if (!M_str_isempty(const_temp))
		transfer_encoding = M_strdup(const_temp);

	const_temp = M_hash_dict_get_direct(headers, "Content-Disposition");
	if (!M_str_isempty(const_temp))
		M_email_attachment_parse_info_attachment(const_temp, &filename);

	const_temp = M_hash_dict_get_direct(headers, "Content-Type");
	if (!M_str_isempty(const_temp)) {
		char *myfilename = NULL;

		content_type = M_email_attachment_parse_info_content_type(const_temp, &myfilename);

		if (M_str_isempty(filename)) {
			filename = myfilename;
		} else {
			M_free(myfilename);
		}
	}

	ret = M_email_part_append_attachment(email, data, len, headers, content_type, transfer_encoding, filename, idx);
	M_free(content_type);
	M_free(filename);
	M_free(transfer_encoding);
	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_t *M_email_create(void)
{
	M_email_t *email;

	email           = M_malloc_zero(sizeof(*email));
	email->headers  = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);

	email->to       = create_address_list();
	email->cc       = create_address_list();
	email->bcc      = create_address_list();
	email->reply_to = M_email_address_create(NULL, NULL, NULL);
	email->from     = M_email_address_create(NULL, NULL, NULL);

	email->parts    = create_part_list();

	return email;
}

void M_email_destroy(M_email_t *email)
{
	if (email == NULL)
		return;

	M_hash_dict_destroy(email->headers);
	M_list_destroy(email->to, M_TRUE);
	M_list_destroy(email->cc, M_TRUE);
	M_list_destroy(email->bcc, M_TRUE);
	M_email_address_destroy(email->reply_to);
	M_email_address_destroy(email->from);
	M_list_destroy(email->parts, M_TRUE);
	M_free(email->preamble);
	M_free(email->epilogue);
	M_free(email->subject);

	M_free(email);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_set_headers(M_email_t *email, const M_hash_dict_t *headers)
{
	M_hash_dict_t             *new_headers;
	M_list_t                  *to       = NULL;
	M_list_t                  *cc       = NULL;
	M_list_t                  *bcc      = NULL;
	M_email_address_t *reply_to = NULL;
	M_email_address_t *from     = NULL;
	char                      *subject  = NULL;
	M_hash_dict_enum_t        *he;
	const char                *key;
	const char                *val;
	M_email_error_t            res      = M_EMAIL_ERROR_SUCCESS;

	if (email == NULL)
		return M_FALSE;

	new_headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);
	to          = create_address_list();
	cc          = create_address_list();
	bcc         = create_address_list();
	reply_to    = M_email_address_create(NULL, NULL, NULL);
	from        = M_email_address_create(NULL, NULL, NULL);

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
		M_email_address_destroy(reply_to);
		M_email_address_destroy(from);
		M_free(subject);
		M_hash_dict_destroy(new_headers);
		return M_FALSE;
	}

	M_hash_dict_destroy(email->headers);
	email->headers = new_headers;

	M_list_destroy(email->to, M_TRUE);
	email->to = to;

	M_list_destroy(email->cc, M_TRUE);
	email->cc = cc;

	M_list_destroy(email->bcc, M_TRUE);
	email->bcc = bcc;

	M_email_address_destroy(email->reply_to);
	email->reply_to = reply_to;

	M_email_address_destroy(email->from);
	email->from = from;

	M_free(email->subject);
	email->subject = subject;

	return M_TRUE;
}

M_bool M_email_headers_insert(M_email_t *email, const char *key, const char *val)
{
	M_list_t                  *alist = NULL;
	M_email_address_t *ad    = NULL;
	M_email_error_t            res;

	if (email == NULL || M_str_isempty(key))
		return M_FALSE;

	if (M_str_caseeq(key, "To") || M_str_caseeq(key, "CC") || M_str_caseeq(key, "BCC")) {
		alist = create_address_list();
		res   = M_email_process_address(val, set_address_list, alist);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			M_list_destroy(alist, M_TRUE);
			return M_FALSE;
		}

		if (M_str_caseeq(key, "To")) {
			M_list_destroy(email->to, M_TRUE);
			email->to = alist;
		} else if (M_str_caseeq(key, "CC")) {
			M_list_destroy(email->cc, M_TRUE);
			email->cc = alist;
		} else if (M_str_caseeq(key, "BCC")) {
			M_list_destroy(email->bcc, M_TRUE);
			email->bcc = alist;
		}

		return M_TRUE;
	}

	if (M_str_caseeq(key, "Reply-To") || M_str_caseeq(key, "From")) {
		ad  = M_email_address_create(NULL, NULL, NULL);
		res = M_email_process_address(val, set_single_address, ad);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			M_email_address_destroy(ad);
			return M_FALSE;
		}

		if (M_str_caseeq(key, "Reply-To")) {
			M_email_address_destroy(email->reply_to);
			email->reply_to = ad;
		} else if (M_str_caseeq(key, "From")) {
			M_email_address_destroy(email->from);
			email->from = ad;
		}

		return M_TRUE;
	}

	if (M_str_caseeq(key, "Subject")) {
		M_free(email->subject);
		email->subject = M_strdup(val);
		return M_TRUE;
	}

	return M_hash_dict_insert(email->headers, key, val);
}

void M_email_headers_remove(M_email_t *email, const char *key)
{
	if (email == NULL || M_str_isempty(key))
		return;

	M_hash_dict_remove(email->headers, key);
}

const M_hash_dict_t *M_email_headers(const M_email_t *email)
{
	if (email == NULL)
		return NULL;

	return email->headers;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_from(const M_email_t *email, char const **group, char const **name, char const **address)
{
	if (email == NULL)
		return M_FALSE;
	return M_email_address_entry(email->from, group, name, address);
}

char *M_email_from_field(const M_email_t *email)
{
	const char *group;
	const char *name;
	const char *address;

	if (email == NULL)
		return NULL;

	M_email_from(email, &group, &name, &address);
	return M_email_write_single_recipient(group, name, address);
}

void M_email_set_from(M_email_t *email, const char *group, const char *name, const char *address)
{
	M_email_address_t *ad;

	if (email == NULL)
		return;

	/* Set before destroy in case the input parameters are
 	 * from M_email_from because something is being added
	 * to the entry. */
	ad = M_email_address_create(group, name, address);
	M_email_address_destroy(email->from);
	email->from = ad;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_to_len(const M_email_t *email)
{
	if (email == NULL)
		return 0;

	return M_list_len(email->to);
}

M_bool M_email_to(const M_email_t *email, size_t idx, char const **group, char const **name, char const **address)
{
	if (email == NULL)
		return M_FALSE;
	return M_email_address_entry((const M_email_address_t *)M_list_at(email->to, idx), group, name, address);
}

char *M_email_to_field(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return M_email_write_recipients(email, M_email_to_len, M_email_to);
}

void M_email_to_append(M_email_t *email, const char *group, const char *name, const char *address)
{
	if (email == NULL)
		return;
	M_list_insert(email->to, M_email_address_create(group, name, address));
}

void M_email_to_remove(M_email_t *email, size_t idx)
{
	if (email == NULL)
		return;
	M_list_remove_at(email->to, idx);
}

void M_email_to_clear(M_email_t *email)
{
	if (email == NULL)
		return;
	M_list_destroy(email->to, M_TRUE);
	email->to = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_cc_len(const M_email_t *email)
{
	if (email == NULL)
		return 0;

	return M_list_len(email->cc);
}

M_bool M_email_cc(const M_email_t *email, size_t idx, char const **group, char const **name, char const **address)
{
	if (email == NULL)
		return M_FALSE;
	return M_email_address_entry((const M_email_address_t *)M_list_at(email->cc, idx), group, name, address);
}

char *M_email_cc_field(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return M_email_write_recipients(email, M_email_cc_len, M_email_cc);
}

void M_email_cc_append(M_email_t *email, const char *group, const char *name, const char *address)
{
	if (email == NULL)
		return;
	M_list_insert(email->cc, M_email_address_create(group, name, address));
}

void M_email_cc_remove(M_email_t *email, size_t idx)
{
	if (email == NULL)
		return;
	M_list_remove_at(email->cc, idx);
}

void M_email_cc_clear(M_email_t *email)
{
	if (email == NULL)
		return;
	M_list_destroy(email->cc, M_TRUE);
	email->cc = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_email_bcc_len(const M_email_t *email)
{
	if (email == NULL)
		return 0;

	return M_list_len(email->bcc);
}

M_bool M_email_bcc(const M_email_t *email, size_t idx, char const **group, char const **name, char const **address)
{
	if (email == NULL)
		return M_FALSE;
	return M_email_address_entry((const M_email_address_t *)M_list_at(email->bcc, idx), group, name, address);
}

char *M_email_bcc_field(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return M_email_write_recipients(email, M_email_bcc_len, M_email_bcc);
}

void M_email_bcc_append(M_email_t *email, const char *group, const char *name, const char *address)
{
	if (email == NULL)
		return;
	M_list_insert(email->bcc, M_email_address_create(group, name, address));
}

void M_email_bcc_remove(M_email_t *email, size_t idx)
{
	if (email == NULL)
		return;
	M_list_remove_at(email->bcc, idx);
}

void M_email_bcc_clear(M_email_t *email)
{
	if (email == NULL)
		return;
	M_list_destroy(email->bcc, M_TRUE);
	email->bcc = create_address_list(); 
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_email_reply_to(const M_email_t *email, char const **group, char const **name, char const **address)
{
	if (email == NULL)
		return M_FALSE;
	return M_email_address_entry(email->reply_to, group, name, address);
}

char *M_email_reply_to_field(const M_email_t *email)
{
	const char *group;
	const char *name;
	const char *address;

	if (email == NULL)
		return NULL;

	M_email_reply_to(email, &group, &name, &address);
	return M_email_write_single_recipient(group, name, address);
}

void M_email_set_reply_to(M_email_t *email, const char *group, const char *name, const char *address)
{
	M_email_address_t *ad;

	if (email == NULL)
		return;

	/* Set before destroy in case the input parameters are
 	 * reply_to M_email_reply_to because something is being added
	 * to the entry. */
	ad = M_email_address_create(group, name, address);
	M_email_address_destroy(email->reply_to);
	email->reply_to = ad;
}

void M_email_reply_to_remove(M_email_t *email)
{
	if (email == NULL)
		return;

	M_email_address_destroy(email->reply_to);
	email->reply_to = M_email_address_create(NULL, NULL, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_email_set_subject(M_email_t *email, const char *subject)
{
	if (email == NULL)
		return;

	M_free(email->subject);
	email->subject = M_strdup(subject);
}

const char *M_email_subject(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return email->subject;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_email_messageid(M_email_t *email, const char *prefix, const char *suffix)
{
	char     id_str[41] = { 0 };
	M_buf_t *buf        = NULL;
	char    *Message_ID = NULL;

	if (email == NULL)
		return;

	M_rand_str(
		NULL,
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789",
		id_str,
		sizeof(id_str) - 1
	);

	buf = M_buf_create();
	M_buf_add_str(buf, prefix);
	M_buf_add_str(buf, id_str);
	M_buf_add_str(buf, suffix);

	Message_ID = M_buf_finish_str(buf, NULL);

	M_email_headers_remove(email, "Message-ID");
	M_email_headers_insert(email, "Message-ID", Message_ID);

	M_free(Message_ID);

}

void M_email_date(M_email_t *email, const char *format)
{
	M_time_localtm_t  ltime;
	char             *date_str;

	if (email == NULL)
		return;

	if (format == NULL)
		format = "%a, %d %b %Y %T %z";

	M_time_tolocal(M_time(), &ltime, NULL);
	date_str = M_time_to_str(format, &ltime);

	M_email_headers_remove(email, "Date");
	M_email_headers_insert(email, "Date", date_str);

	M_free(date_str);

}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_email_preamble(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return email->preamble;
}

void M_email_set_preamble(M_email_t *email, const char *data, size_t len)
{
	if (email == NULL)
		return;

	M_free(email->preamble);
	email->preamble = M_strdup_max(data, len);
}

const char *M_email_epilouge(const M_email_t *email)
{
	if (email == NULL)
		return NULL;
	return email->epilogue;
}

void M_email_set_epilouge(M_email_t *email, const char *data, size_t len)
{
	if (email == NULL)
		return;

	M_free(email->epilogue);
	email->epilogue = M_strdup_max(data, len);
}

M_bool M_email_part_append(M_email_t *email, const char *data, size_t len, const M_hash_dict_t *headers, size_t *idx)
{
	M_email_part_t *part;
	M_hash_dict_enum_t     *he;
	const char             *key;
	const char             *val;
	size_t                  myidx;

	if (idx == NULL)
		idx = &myidx;
	*idx = 0;

	if (email == NULL)
		return M_FALSE;

	if (append_part_is_attachment(headers))
		return parse_insert_attachment(email, data, len, headers, idx);

	part = M_email_part_create();
	M_buf_add_bytes(part->data, data, len);

	if (headers != NULL) {
		M_hash_dict_enumerate(headers, &he);
		while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
			M_hash_dict_insert(part->headers, key, val);
		}
		M_hash_dict_enumerate_free(he);
	}

	if (!M_list_insert(email->parts, part)) {
		M_email_part_destroy(part);
		return M_FALSE;
	}

	*idx = M_list_len(email->parts)-1;
	return M_TRUE;
}

M_bool M_email_part_append_attachment(M_email_t *email, const char *data, size_t len, const M_hash_dict_t *headers, const char *content_type, const char *transfer_encoding, const char *filename, size_t *idx)
{
	M_email_part_t *part;
	M_hash_dict_enum_t     *he;
	const char             *key;
	const char             *val;
	size_t                  myidx;

	if (idx == NULL)
		idx = &myidx;
	*idx = 0;

	if (email == NULL)
		return M_FALSE;

	part                    = M_email_part_create();
	M_buf_add_bytes(part->data, data, len);
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

	if (!M_list_insert(email->parts, part)) {
		M_email_part_destroy(part);
		return M_FALSE;
	}

	*idx = M_list_len(email->parts)-1;
	return M_TRUE;
}

M_bool M_email_part_append_data(M_email_t *email, size_t idx, const char *data, size_t len)
{
	M_email_part_t *part;

	if (email == NULL)
		return M_FALSE;

	part = (M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return M_FALSE;

	M_buf_add_bytes(part->data, data, len);
	return M_TRUE;
}

M_bool M_email_part_set_data(M_email_t *email, size_t idx, const char *data, size_t len)
{
	M_email_part_t *part;

	if (email == NULL)
		return M_FALSE;

	part = (M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return M_FALSE;

	M_buf_truncate(part->data, 0);
	M_buf_add_bytes(part->data, data, len);

	return M_TRUE;
}

size_t M_email_parts_len(const M_email_t *email)
{
	if (email == NULL)
		return 0;
	return M_list_len(email->parts);
}

void M_email_parts_clear(M_email_t *email)
{
	if (email == NULL)
		return;
	M_list_destroy(email->parts, M_TRUE);
	email->parts = create_part_list();
}

const char *M_email_part_data(const M_email_t *email, size_t idx)
{
	const M_email_part_t *part;

	if (email == NULL)
		return NULL;

	part = (const M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return NULL;
	return M_buf_peek(part->data);
}

const M_hash_dict_t *M_email_part_headers(const M_email_t *email, size_t idx)
{
	const M_email_part_t *part;

	if (email == NULL)
		return NULL;

	part = (const M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return NULL;
	return part->headers;
}

M_bool M_email_part_is_attachmenet(const M_email_t *email, size_t idx)
{
	const M_email_part_t *part;

	if (email == NULL)
		return M_FALSE;

	part = (const M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return M_FALSE;
	return part->is_attachment;
}

M_bool M_email_part_attachment_info(const M_email_t *email, size_t idx, char const **content_type, char const **transfer_encoding, char const **filename)
{
	const M_email_part_t *part;

	if (content_type != NULL)
		*content_type = NULL;
	if (transfer_encoding != NULL)
		*transfer_encoding = NULL;
	if (filename != NULL)
		*filename = NULL;

	if (email == NULL)
		return M_FALSE;

	part = (const M_email_part_t *)M_list_at(email->parts, idx);
	if (part == NULL)
		return M_FALSE;

	if (content_type != NULL)
		*content_type = part->content_type;
	if (transfer_encoding != NULL)
		*transfer_encoding = part->transfer_encoding;
	if (filename != NULL)
		*filename = part->filename;

	return M_TRUE;
}

void M_email_part_remove(M_email_t *email, size_t idx)
{
	if (email == NULL)
		return;

	M_list_remove_at(email->parts, idx);
}

void M_email_set_mixed_multipart(M_email_t *email, M_bool is_mixed_multipart)
{
	if (email == NULL)
		return;
	email->is_mixed_multipart = is_mixed_multipart;
}

M_bool M_email_is_mixed_multipart(const M_email_t *email)
{
	if (email == NULL)
		return M_FALSE;
	return email->is_mixed_multipart;
}
