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
#include <mstdlib/mstdlib_formats.h>
#include "xml/m_xml_entities.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Identify the various tags we can parse */
typedef enum {
	M_XML_TAG_PROCESSING_INSTRUCTION = 1,
	M_XML_TAG_COMMENT                = 2,
	M_XML_TAG_ELEMENT_START          = 3,
	M_XML_TAG_ELEMENT_END            = 4,
	M_XML_TAG_ELEMENT_EMPTY          = 5,
	M_XML_TAG_CDATA                  = 6,
	M_XML_TAG_DECLARATION            = 7
} M_xml_reader_tags_t;

typedef struct {
	char                *name;          /*!< Named tag                            */
	M_xml_reader_tags_t  type;          /*!< Type of XML tag being processed      */
	size_t               processed_len; /*!< Number of bytes processed            */
	size_t               tag_len;       /*!< Number of bytes in total tag size    */
	size_t               len_left;      /*!< Number of bytes left to be processed */
} M_xml_reader_tag_info_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/*! Scan data provided for an unquoted matching character. Honors both single
 *  and double quotes.
 *  \param[in] data     Data to scan
 *  \param[in] data_len Length of data to scan
 *  \param[in] ch       Search character
 *  \returns NULL on failure, pointer to character on success
 */
static char *M_xml_read_find_unquoted_chr(const char *data, size_t data_len, int ch)
{
	size_t i;
	int on_quote = 0;

	for (i=0; i<data_len; i++) {
		if (data[i] == '\'' || data[i] == '"') {
			if (on_quote) {
				if (data[i] == on_quote)
					on_quote = 0;
			} else {
				on_quote = data[i];
			}
		} else if (data[i] == ch) {
			if (!on_quote)
				return ((char *)((M_uintptr)(data+i)));
		}
	}
	return NULL;
}

/*! Scan data in reverse for a character, skip whitepace, and break out
 *  on a non-whitespace character that is not our ch
 * \param[in] data     Data to scan
 * \param[in] data_len Length of data to scan
 * \param[in] ch       Search character
 * \returns NULL on failure, point to ch on success */
static const char *M_xml_read_find_nonws_ch_reverse(const char *data, size_t data_len, int ch)
{
	size_t i;

	for (i=data_len; i-->0; ) {
		if (M_chr_isspace(data[i]))
			continue;
		if (data[i] == ch)
			return data + i;
		/* Break out on any other ch */
		break;
	}

	return NULL;
}

/*! Find the ending marker for this tag
 *  \param[in]     data      Data to be processed
 *  \param[in]     data_len  Length of data to be processed
 *  \param[in,out] tag_type  Type of tag we're locating an ending for. If this is
 *                           an element start tag, but we realize this is an
 *                           empty element, this function will change the tag type.
 *  \param[out]    len_left  Length of data up to, but not including the end tag
 *  \param[out]    error     Error buffer
 *  \param[in]     error_len Length of error buffer
 *  \returns 0 on failure, length of data including the end tag
 */
static size_t M_xml_read_tag_end(const char *data, size_t data_len, M_xml_reader_tags_t *tag_type, size_t *len_left, M_xml_error_t *error)
{
	const char *end_tag;
	const char *ptr;
	size_t      len_processed;

	switch (*tag_type) {
		case M_XML_TAG_COMMENT:
			end_tag      = "-->";
			break;
		case M_XML_TAG_CDATA:
			end_tag      = "]]>";
			break;
		default:
			end_tag      = ">";
	}

	if (M_str_len(end_tag) > 1) {
		/* Don't honor quotes, just find the end */
		ptr = M_mem_mem(data, data_len, end_tag, M_str_len(end_tag));
	} else {
		ptr = M_xml_read_find_unquoted_chr(data, data_len, *end_tag);
	}
	if (ptr == NULL) {
		*error = M_XML_ERROR_MISSING_CLOSE_TAG;
		return 0;
	}

	*len_left     = (size_t)(ptr - data);
	len_processed = *len_left + M_str_len(end_tag);

	/* On declarations, scan back to the '?' */
	if (*tag_type == M_XML_TAG_PROCESSING_INSTRUCTION) {
		ptr = M_xml_read_find_nonws_ch_reverse(data, *len_left, '?');
		if (ptr == NULL) {
			*error = M_XML_ERROR_MISSING_PROCESSING_INSTRUCTION_END;
			return 0;
		}
		*len_left = (size_t)(ptr - data);
	}

	/* On M_XML_TAG_ELEMENT_START, see if this is really M_XML_TAG_ELEMENT_EMPTY
	 * by scanning back for a '/' */
	if (*tag_type == M_XML_TAG_ELEMENT_START) {
		ptr = M_xml_read_find_nonws_ch_reverse(data, *len_left, '/');
		if (ptr != NULL) {
			*tag_type = M_XML_TAG_ELEMENT_EMPTY;
			*len_left = (size_t)(ptr - data);
		}
	}

	return len_processed;
}

/*! Read the tag name in.  Only stop on whitespace or the end
 *  of the tag, whichever comes first
 *  \param[in] data Data to be processed
 *  \param[in] data_len Length of data to be processed
 *  \returns NULL on failure, allocated pointer to name on success */
static char *M_xml_read_name(const char *data, size_t data_len)
{
	size_t len;
	size_t i;

	/* Scan till first whitespace, or end, whichever comes first */
	for (i=0; i<data_len; i++) {
		if (M_chr_isspace(data[i]))
			break;
	}
	len = i;

	return len > 0 ? M_strdup_max(data, len) : NULL;
}

/*! Gather information about the XML tag encountered such as the type of
 *  tag, it's length, it's name (if applicable), how much of it has been
 *  consumed by this process and how much is left to be consumed
 *  \param[in]  data      Data to be processed
 *  \param[in]  data_len  Length of Data to be processed
 *  \param[out] info      Info structure holding result data
 *  \param[out] error     Error buffer
 *  \param[in]  error_len Length of error buffer
 *  \returns M_TRUE on success, M_FALSE on failure
 */
static M_bool M_xml_read_tag_info(const char *data, size_t data_len, M_xml_reader_tag_info_t *info, M_xml_error_t *error)
{
	const char *ptr;
	size_t      ptr_len;

	M_mem_set(info, 0, sizeof(*info));

	ptr     = data;
	ptr_len = data_len;

	if (*ptr != '<') {
		*error = M_XML_ERROR_INVALID_START_TAG;
		return M_FALSE;
	}

	/* Skip opening bracket */
	ptr++;
	ptr_len--;

	/* Skip any whitespace (yeah, don't think the spec requires this) */
	while (ptr_len && M_chr_isspace(*ptr)) {
		ptr++;
		ptr_len--;
	}

	if (!ptr_len) {
		*error = M_XML_ERROR_EMPTY_START_TAG;
		return M_FALSE;
	}

	/* Determine tag type */
	switch (*ptr) {
		case '/':
			info->type = M_XML_TAG_ELEMENT_END;
			ptr++;
			ptr_len--;
			break;
		case '?':
			info->type = M_XML_TAG_PROCESSING_INSTRUCTION;
			ptr++;
			ptr_len--;
			break;
		case '<':
			*error = M_XML_ERROR_INVALID_CHAR_IN_START_TAG;
			return M_FALSE;
		case '!':
			ptr++;
			ptr_len--;

			/* Skip any whitespace again (yeah, don't think the spec requires this) */
			while (ptr_len && M_chr_isspace(*ptr)) {
				ptr++;
				ptr_len--;
			}
			if (!ptr_len) {
				*error = M_XML_ERROR_MISSING_DECLARATION_NAME;
				return M_FALSE;
			}

			if (ptr_len >= 2 && M_mem_eq(ptr, "--", 2)) {
				/* Check for <!-- */
				info->type = M_XML_TAG_COMMENT;
				ptr     += 2;
				ptr_len -= 2;
			} else if (ptr_len >= 7 && M_mem_eq(ptr, "[CDATA[", 7)) {
				/* Check for <![CDATA[ */
				info->type = M_XML_TAG_CDATA;
				ptr     += 7;
				ptr_len -= 7;
			} else {
				/* Could be <!DOCTYPE, <!ELEMENT, <!ATTLIST, <!ENTITY */
				info->type = M_XML_TAG_DECLARATION;
			}
			break;
		default:
			info->type = M_XML_TAG_ELEMENT_START;
			break;
	}

	/* Skip leading whitespace */
	if (info->type == M_XML_TAG_ELEMENT_END || info->type == M_XML_TAG_PROCESSING_INSTRUCTION) {
		while (ptr_len && M_chr_isspace(*ptr)) {
			ptr++;
			ptr_len--;
		}
	}

	info->processed_len = (size_t)(ptr - data);

	info->tag_len = M_xml_read_tag_end(ptr, ptr_len, &info->type, &info->len_left, error);
	if (info->tag_len == 0)
		return M_FALSE;
	info->tag_len += info->processed_len;

	if (info->type != M_XML_TAG_CDATA && info->type != M_XML_TAG_COMMENT) {
		/* Parse name */
		info->name = M_xml_read_name(ptr, info->len_left);
		if (info->name == NULL) {
			*error = M_XML_ERROR_INVALID_START_TAG;
			return M_FALSE;
		}
		info->processed_len += M_str_len(info->name);
		info->len_left      -= M_str_len(info->name);
	}

	return M_TRUE;
}

/*! Strip extra whitespace from the attribute list, including whitespace
 *  on either side of the '='
 * \param[in] data     Data to strip
 * \param[in] data_len Length of data to be stripped
 * \returns allocated buffer containing stripped data, or NULL if it was
 *          all whitespace
 */
static char *M_xml_read_attribute_strip_extra_whitespace(const char *data, size_t data_len)
{
	char    *buf;
	size_t   i;
	size_t   cnt           = 0;
	int      on_quote      = 0;
	int      request_space = 0;

	/* Skip leading whitespace */
	while (data_len && M_chr_isspace(*data)) {
		data++;
		data_len--;
	}

	/* Nothing to parse */
	if (data_len == 0)
		return NULL;

	/* Trim off trailing whitespace */
	for ( ; data_len > 0 && M_chr_isspace(data[data_len-1]); data_len--);

	/* Trim all extra spaces not inside quotes */
	buf = M_malloc(data_len + 1);
	for (i=0; i<data_len; i++) {
		if (on_quote) {
			if (data[i] == on_quote) {
				on_quote = 0;
			}
			buf[cnt++] = data[i];
		} else if (data[i] == '\'' || data[i] == '"') {
			if (request_space == 1)
				buf[cnt++] = ' ';
			request_space = 0;
			on_quote      = data[i];
			buf[cnt++]    = data[i];
		} else if (M_chr_isspace(data[i])) {
			/* If we just hit an equal sign, we can't have
			 * any spaces, so look for that, otherwise request
			 * on the next character that we prepend a space
			 */
			if (request_space != -1)
				request_space = 1;
		} else if (data[i] == '=') {
			/* No spaces allowed after an equal sign, also do not
			 * honor a request_space here */
			request_space = -1;
			buf[cnt++] = data[i];
		} else {
			/* Not quoted, not a space, not an equal sign */
			if (request_space == 1) {
				buf[cnt++] = ' ';
			}
			buf[cnt++] = data[i];
			request_space = 0;
		}
	}

	buf[cnt] = 0;
	return buf;
}


static char **M_xml_read_attribute_explode(int ch, char *data, size_t *num)
{
	char   **out;
	char    *ptr;
	size_t   num_strs;

	/* Count strings */
	num_strs = 1;
	ptr      = data;
	while (ptr != NULL) {
		ptr = M_xml_read_find_unquoted_chr(ptr, M_str_len(ptr), ch);
		if (ptr != NULL) {
			num_strs++;
			ptr++;
		}
	}

	/* Allocate */
	out      = M_malloc(num_strs * sizeof(*out));
	out[0]   = data;

	/* Set pointers, NULL terminate the spaces */
	ptr      = data;
	num_strs = 1;
	while (ptr != NULL) {
		ptr = M_xml_read_find_unquoted_chr(ptr, M_str_len(ptr), ch);
		if (ptr != NULL) {
			*ptr = 0;
			ptr++;
			out[num_strs++] = ptr;
		}
	}

	*num = num_strs;
	return out;
}


/*! Strip surrounding quotes from string.
 *  \param data String to strip
 *  \returns NULL if no quotes to parse, on success, allocated buffer with stripped data
 */
static char *M_xml_read_strip_surrounding_quotes(const char *data)
{
	size_t data_len;

	if (data == NULL)
		return NULL;

	if (data[0] != '\'' && data[0] != '\"')
		return NULL;

	/* Last quote must match first quote */
	data_len = M_str_len(data);
	if (data[0] != data[data_len-1])
		return NULL;

	return M_strdup_max(data+1, data_len-2);
}


/*! Parse attributes into key/value pairs
 *  \param[in] node     Node to add attributes
 *  \param[in] data     Data to parse
 *  \param[in] data_len Length of data to parse
 *  \returns M_TRUE on success, M_FALSE on failure
 */
static M_bool M_xml_read_tag_attributes(M_xml_node_t *node, const char *data, size_t data_len, M_uint32 flags, M_xml_error_t *error)
{
	char       *sdata;
	char      **kvdata;
	size_t      num_kvdata = 0;
	char      **keyval;
	size_t      num_keyval;
	size_t      i;
	char       *keytemp;
	char       *valtemp;
	char       *decoded_val;
	const char *key;
	const char *val;

	/* make it so this is a sanitized data set. just key/value pairs, possibly
	 * quoted, separated exactly one space */
	sdata = M_xml_read_attribute_strip_extra_whitespace(data, data_len);
	if (sdata == NULL)
		return M_TRUE;

	kvdata = M_xml_read_attribute_explode(' ', sdata, &num_kvdata);
	for (i=0; i<num_kvdata; i++) {
		num_keyval = 0;
		keyval = M_xml_read_attribute_explode('=', kvdata[i], &num_keyval);
		if (num_keyval != 0) {
			key     = keyval[0];
			keytemp = M_xml_read_strip_surrounding_quotes(key);
			if (keytemp != NULL)
				key = keytemp;

			val         = NULL;
			decoded_val = NULL;
			if (num_keyval > 1)
				val = keyval[1];
			valtemp = M_xml_read_strip_surrounding_quotes(val);
			if (valtemp != NULL)
				val = valtemp;
			if (!(flags & M_XML_READER_DONT_DECODE_ATTRS)) {
				decoded_val = M_xml_attribute_decode(val, M_str_len(val));
			}
			if (!M_xml_node_insert_attribute(node, key, decoded_val!=NULL?decoded_val:val, 0, M_FALSE)) {
				*error = M_XML_ERROR_ATTR_EXISTS;
				M_free(keytemp);
				M_free(valtemp);
				M_free(decoded_val);

				M_free(keyval);

				M_free(sdata);
				M_free(kvdata);
				return M_FALSE;
			}
			M_free(keytemp);
			M_free(valtemp);
			M_free(decoded_val);
		}
		M_free(keyval);
	}
	M_free(sdata);
	M_free(kvdata);

	return M_TRUE;
}

/*! Handle logic for tag encountered
 * \return M_TRUE on success, M_FALSE on failure
 */
static M_bool M_xml_read_tag_process(M_xml_node_t **node, const char *data, size_t data_len, M_xml_reader_tag_info_t *info, M_uint32 flags, M_xml_error_t *error)
{
	M_xml_node_t *new_node = NULL;
	char         *text     = NULL;

	switch (info->type) {
		case M_XML_TAG_PROCESSING_INSTRUCTION:
		case M_XML_TAG_DECLARATION:
		case M_XML_TAG_ELEMENT_START:
		case M_XML_TAG_ELEMENT_EMPTY:
			if (info->type == M_XML_TAG_PROCESSING_INSTRUCTION) {
				new_node = M_xml_create_processing_instruction(info->name, *node);
			} else if (info->type == M_XML_TAG_DECLARATION) {
				new_node = M_xml_create_declaration(info->name, *node);
			} else {
				new_node = M_xml_create_element(info->name, *node);
			}
			if (new_node == NULL) {
				*error = M_XML_ERROR_GENERIC;
				return M_FALSE;
			}

			if (info->type == M_XML_TAG_DECLARATION) {
				if (!M_xml_node_set_tag_data(new_node, data)) {
					*error = M_XML_ERROR_GENERIC;
					M_xml_node_destroy(new_node);
					return M_FALSE;
				}
			} else {
				if (!M_xml_read_tag_attributes(new_node, data, data_len, flags, error)) {
					M_xml_node_destroy(new_node);
					return M_FALSE;
				}
			}

			/* On the start of a new element is the only instance
			 * where we'll change the node pointer */
			if (info->type == M_XML_TAG_ELEMENT_START) {
				*node = new_node;
			}
			break;

		case M_XML_TAG_ELEMENT_END:
			if (M_xml_node_type(*node) != M_XML_NODE_TYPE_ELEMENT) {
				*error = M_XML_ERROR_INELIGIBLE_FOR_CLOSE;
				return M_FALSE;
			}
			if ((flags & M_XML_READER_TAG_CASECMP && !M_str_caseeq(info->name, M_xml_node_name(*node))) || (!(flags & M_XML_READER_TAG_CASECMP) && !M_str_eq(info->name, M_xml_node_name(*node)))) {
				*error = M_XML_ERROR_UNEXPECTED_CLOSE;
				return M_FALSE;
			}
			/* We just closed this element, reset the pointer back one */
			*node = M_xml_node_parent(*node);
			break;

		case M_XML_TAG_CDATA:
			/* Standard text data would be encoded, so we need to treat this as encoded */
			if (!(flags & M_XML_READER_DONT_DECODE_TEXT)) {
				text = M_xml_entities_decode(data, data_len);
			}
			new_node = M_xml_create_text(text!=NULL?text:data, 0, *node);
			M_free(text);
			if (new_node == NULL) {
				*error = M_XML_ERROR_GENERIC;
				return M_FALSE;
			}
			break;

		case M_XML_TAG_COMMENT:
			if (flags & M_XML_READER_IGNORE_COMMENTS)
				break;
			text     = M_strdup_trim_max(data, data_len);
			new_node = M_xml_create_comment(text, *node);
			M_free(text);
			if (new_node == NULL) {
				*error = M_XML_ERROR_GENERIC;
				return M_FALSE;
			}
			break;
	}

	return M_TRUE;
}

/*! Parse an XML tag, examples of XML tags include:
 *    <?XXX?>         -- M_XML_TAG_PROCESSING_INSTRUCTION
 *    <!--XXX-->      -- M_XML_TAG_COMMENT
 *    <XXX>           -- M_XML_TAG_ELEMENT
 *    <XXX/>          -- M_XML_TAG_ELEMENT (Auto-close)
 *    </XXX>          -- M_XML_TAG_ELEMENT_CLOSE
 *    <![CDATA[XXX]]> -- M_XML_TAG_CDATA
 * \param[in,out] branch   Current XML branch level, should be updated for the current
 *                         position in the tree.
 * \param[in]     data     Data to evaluate
 * \param[in]     data_len Length of data to be evaluated
 * \param[out]    error    Error code
 * \return length of parsed data on success, 0 on failure.
 */
static size_t M_xml_read_tag(M_xml_node_t **node, const char *data, size_t data_len, M_uint32 flags, M_xml_error_t *error)
{
	M_xml_reader_tag_info_t info;

	M_mem_set(&info, 0, sizeof(info));
	if (!M_xml_read_tag_info(data, data_len, &info, error)) {
		M_free(info.name);
		return 0;
	}

	if (!M_xml_read_tag_process(node, data+info.processed_len, info.len_left, &info, flags, error)) {
		M_free(info.name);
		return 0;
	}
	M_free(info.name);
	return info.tag_len;
}

/*! Scans the data provided for the first XML start character (<), then
 *  trims any beginning or trailing whitespace and adds the data to
 *  the current branch location
 *  \param[in]  node     XML node where the data is to be appended
 *  \param[in]  data     Data to evaluate
 *  \param[in]  data_len Entire length of data to be evaluated
 *  \param[out] error    Error code
 *  \returns length of parsed data on success, 0 on failure
 */
static size_t M_xml_read_text(M_xml_node_t *node, const char *data, size_t data_len, M_uint32 flags, M_xml_error_t *error)
{
	const char *ptr;
	char       *branch_data;
	char       *decoded_branch_data = NULL;
	size_t      text_len;
	size_t      processed_len;

	ptr = M_mem_chr(data, '<', data_len);
	if (ptr == NULL) {
		processed_len = data_len;
	} else {
		processed_len = (size_t)(ptr - data);
	}

	text_len = processed_len;
	/* Trim off beginning whitespace */
	while (text_len > 0 && M_chr_isspace(*data)) {
		data++;
		text_len--;
	}

	/* Trim off trailing whitespace */
	for ( ; M_chr_isspace(data[text_len-1]) && text_len > 0; text_len--)
		;

	branch_data = M_malloc(text_len + 1);
	M_mem_copy(branch_data, data, text_len);
	branch_data[text_len] = 0;
	if (!(flags & M_XML_READER_DONT_DECODE_TEXT)) {
		decoded_branch_data = M_xml_entities_decode(branch_data, text_len);
	}
	if (!M_xml_create_text(decoded_branch_data!=NULL?decoded_branch_data:branch_data, 0, node)) {
		*error = M_XML_ERROR_GENERIC;
		M_free(branch_data);
		M_free(decoded_branch_data);
		return 0;
	}
	M_free(branch_data);
	M_free(decoded_branch_data);

	return processed_len;
}

/*! Scan tree to see if an actual XML element has been parsed.
 *
 * \param[in] doc XML doc.
 *
 * \returns M_TRUE if found, M_FALSE if not.
 */
static M_bool M_xml_doc_has_element(M_xml_node_t *doc)
{
	size_t len;
	size_t i;

	if (doc == NULL || M_xml_node_type(doc) != M_XML_NODE_TYPE_DOC)
		return M_FALSE;

	len = M_xml_node_num_children(doc);
	for (i=0; i<len; i++) {
		if (M_xml_node_type(M_xml_node_child(doc, i)) == M_XML_NODE_TYPE_ELEMENT) {
			return M_TRUE;
		}
	}
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_xml_node_t *M_xml_read(const char *data, size_t data_len, M_uint32 flags, size_t *processed_len, M_xml_error_t *error, size_t *error_line, size_t *error_pos)
{
	M_xml_node_t  *doc;
	M_xml_node_t  *curr_level;
	M_xml_error_t  myerror;
	size_t         myerror_line;
	size_t         myerror_pos;
	size_t         i;
	size_t         retval;

	if (error == NULL)
		error = &myerror;
	*error = M_XML_ERROR_SUCCESS;
	if (error_line == NULL)
		error_line = &myerror_line;
	*error_line = 0;
	if (error_pos == NULL)
		error_pos = &myerror_pos;
	*error_pos = 0;

	if (data == NULL || data_len == 0) {
		*error = M_XML_ERROR_MISUSE;
		return NULL;
	}

	doc        = M_xml_create_doc();
	curr_level = doc;

	for (i=0; i<data_len; i++) {
		/* Skip whitespace */
		if (M_chr_isspace(data[i])) {
			continue;
		}

		/* If we're back at the doc, and we have a real child, and it's
		 * after we've eaten all whitespace ... we're done.
		 */
		if (curr_level == doc && M_xml_doc_has_element(doc)) {
			if (processed_len) {
				/* Stop processing, we'll return how much was processed. In cases where there are multiple documents
 				 * in the stream the caller will know there is more data and run another parse. */
				break;
			} else {
				/* Multiple roots are invalid... */
				*error = M_XML_ERROR_EXPECTED_END;
				*error_pos = i;
				M_xml_node_destroy(doc);
				return NULL;
			}
		}

		if (data[i] == '<') {
			/* Parse <?XXX?>, <!--XXX-->, <XXX>, <XXX/>, </XXX>, <![CDATA[XXX]]> */
			retval = M_xml_read_tag(&curr_level, data+i, data_len-i, flags, error);
			if (retval == 0) {
				*error_pos = i;
				M_xml_node_destroy(doc);
				return NULL;
			}
			i += (retval - 1);
		} else {
			/* Parse text up to next '<' */
			retval = M_xml_read_text(curr_level, data+i, data_len-i, flags, error);
			if (retval == 0) {
				M_xml_node_destroy(doc);
				return NULL;
			}
			i += (retval - 1);
		}
	}

	if (curr_level != doc) {
		*error     = M_XML_ERROR_MISSING_CLOSE_TAG;
		*error_pos = i;
		M_xml_node_destroy(doc);
		return NULL;
	}

	if (!M_xml_doc_has_element(doc)) {
		*error     = M_XML_ERROR_NO_ELEMENTS;
		*error_pos = i;
		M_xml_node_destroy(doc);
		return NULL;
	}

	if (processed_len != NULL)
		*processed_len = i;

	return doc;
}

M_xml_node_t *M_xml_read_file(const char *path, M_uint32 flags, size_t max_read, M_xml_error_t *error, size_t *error_line, size_t *error_pos)
{
	char         *buf;
	M_xml_node_t *node;
	size_t        bytes_read;
	M_fs_error_t  res;
	
	res = M_fs_file_read_bytes(path, max_read, (unsigned char **)&buf, &bytes_read);
	if (res != M_FS_ERROR_SUCCESS || buf == NULL) {
		if (error != NULL) {
			*error = M_XML_ERROR_GENERIC;
		}
		M_free(buf);
		return NULL;
	}

	node = M_xml_read(buf, bytes_read, flags, NULL, error, error_line, error_pos);
	M_free(buf);
	return node;
}
