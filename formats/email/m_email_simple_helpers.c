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

M_email_error_t M_email_simple_split_header_body(const char *message, M_hash_dict_t **headers, char **body)
{
	M_hash_dict_t  *myheaders;
	M_parser_t     *parser;
	header_state_t  hsres = HEADER_STATE_MOREDATA;

	if (headers != NULL)
		*headers = NULL;
	if (body != NULL)
		*body = NULL;

	if (M_str_isempty(message))
		return M_EMAIL_ERROR_MOREDATA;

	parser    = M_parser_create_const((const unsigned char *)message, M_str_len(message), M_PARSER_FLAG_NONE);
	myheaders = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);

	do {
		char *key = NULL;
		char *val = NULL;

		hsres = M_email_header_get_next(parser, &key, &val);
		switch (hsres) {
			case HEADER_STATE_FAIL:
			case HEADER_STATE_END:
			case HEADER_STATE_MOREDATA:
				goto end_of_header;
			case HEADER_STATE_SUCCESS:
				break;
		}

		M_hash_dict_insert(myheaders, key, val);

		M_free(key);
		M_free(val);
	} while (M_parser_len(parser) != 0);

end_of_header:
	if (hsres == HEADER_STATE_END) {
		if (headers != NULL) {
			*headers  = myheaders;
			myheaders = NULL;
		}
		if (body != NULL) {
			*body = M_parser_read_strdup(parser, M_parser_len(parser));
		}
	}
	M_hash_dict_destroy(myheaders);

	M_parser_destroy(parser);

	if (hsres == HEADER_STATE_MOREDATA)
		return M_EMAIL_ERROR_MOREDATA;
	if (hsres == HEADER_STATE_END)
		return M_EMAIL_ERROR_SUCCESS;
	return M_EMAIL_ERROR_HEADER_INVALID;
}
