/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_XML_XPATH_MATCH_TYPE_INVALID = 0,
	M_XML_XPATH_MATCH_TYPE_TAG,
	M_XML_XPATH_MATCH_TYPE_ATTR_ANY,
	M_XML_XPATH_MATCH_TYPE_ATTR_HAS,
	M_XML_XPATH_MATCH_TYPE_ATTR_VAL,
	M_XML_XPATH_MATCH_TYPE_POS,
	M_XML_XPATH_MATCH_TYPE_TEXT
} M_xml_xpath_match_type_t;

typedef enum {
	M_XML_XPATH_POS_EQUALITY_EQ = 0,
	M_XML_XPATH_POS_EQUALITY_LTE,
	M_XML_XPATH_POS_EQUALITY_GTE,
	M_XML_XPATH_POS_EQUALITY_LT,
	M_XML_XPATH_POS_EQUALITY_GT,
} M_xml_xpath_pos_equality_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void M_xml_xpath_search(M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches);

static M_xml_node_t *M_xml_node_find_doc(M_xml_node_t *node)
{
	while (node != NULL && M_xml_node_parent(node) != NULL)
		node = M_xml_node_parent(node);
	return node;
}

static M_xml_xpath_match_type_t M_xml_xpath_search_sement_type(const char *seg)
{
	if (seg == NULL || *seg == '\0')
		return M_XML_XPATH_MATCH_TYPE_TAG;

	if (*seg == '[') {
		/* Invalid predicate. */
		if (M_str_len(seg) < 3) {
			return M_XML_XPATH_MATCH_TYPE_INVALID;
		}
		if (M_str_eq(seg, "[@*]")) {
			return M_XML_XPATH_MATCH_TYPE_ATTR_ANY;
		}
		if (*(seg+1) == '@') {
			if (M_str_chr(seg, '=') == NULL) {
				return M_XML_XPATH_MATCH_TYPE_ATTR_HAS;
			}
			return M_XML_XPATH_MATCH_TYPE_ATTR_VAL;
		}
		return M_XML_XPATH_MATCH_TYPE_POS;
	}

	if (M_str_eq(seg, "text()")) {
		return M_XML_XPATH_MATCH_TYPE_TEXT;
	}

	return M_XML_XPATH_MATCH_TYPE_TAG;
}

static M_bool M_xml_xpath_search_tag_eq(M_xml_node_t *node, const char *tag, M_uint32 flags)
{
	const char *name;
	size_t      len;
	char       *p;

	name = M_xml_node_name(node);

	len = M_str_len(tag);
	if (len > 2 && M_str_eq_max(tag, "*:", 2)) {
		tag += 2;
		p = M_str_chr(name, ':');
		if (p != NULL) {
			name = p+1;
		}
	}

	if (M_str_eq(tag, "*") ||
		(flags & M_XML_READER_TAG_CASECMP && M_str_caseeq(tag, name)) ||
		(!(flags & M_XML_READER_TAG_CASECMP) && M_str_eq(tag, name)))
	{
		return M_TRUE;
	}

	return M_FALSE;
}

static M_bool M_xml_xpath_search_match_node_pos_offset(const char *val, size_t val_len, size_t array_len, size_t *out_pos, size_t *out_max)
{
	M_buf_t                    *buf;
	char                       *myval;
	char                       *p;
	char                        sign;
	M_xml_xpath_pos_equality_t  equality      = M_XML_XPATH_POS_EQUALITY_EQ;
	M_int64                     offset        = 0;
	const size_t                wlen_position = 10; /* M_str_len("position()"); */
	const size_t                wlen_last     = 6; /* M_str_len("last()"); */
	M_bool                      has_last      = M_FALSE;

	*out_pos = 0;
	*out_max = 1;

	myval = M_strdup_trim(val);

	/* Checking for position() and last() are really relaxed. These could
 	 * skip invalid data when it really should error. */

	/* Check for the position function */
	p = M_str_str(myval, "position()");
	if (p != NULL) {
		/* Move past the text. */
		p += wlen_position;
		if (M_str_isempty(p)) {
			/* Position requires an argument. */
			M_free(myval);
			return M_FALSE;
		}
		/* Determine what kind of equality is being used. Store and move past it. */
		if ((p = M_str_str(myval, "<=")) != NULL) {
			p += 2;
			equality = M_XML_XPATH_POS_EQUALITY_LTE;
		} else if ((p = M_str_str(myval, ">=")) != NULL) {
			p += 2;
			equality = M_XML_XPATH_POS_EQUALITY_GTE;
		} else if ((p = M_str_chr(myval, '<')) != NULL) {
			p++;
			equality = M_XML_XPATH_POS_EQUALITY_LT;
		} else if ((p = M_str_chr(myval, '>')) != NULL) {
			p++;
			equality = M_XML_XPATH_POS_EQUALITY_GT;
		} else if ((p = M_str_chr(myval, '=')) != NULL) {
			/* '=' needs to come last but before else because 
 			 * str_chr would match "<=" or ">=" too. */
			p++;
			equality = M_XML_XPATH_POS_EQUALITY_EQ;
		} else {
			/* Position requires modifier. */
			M_free(myval);
			return M_FALSE;
		}

		/* Remove position() from the value so we can convert it into an int later */
		buf   = M_buf_create();
		M_buf_add_str(buf, p);
		M_free(myval);
		myval = M_str_trim(M_buf_finish_str(buf, NULL));
	}

	/* Check if last is used as an argument. */
	p = M_str_str(myval, "last()");
	if (p != NULL) {
		/* Remove last() from the value so we can convert it into an int later */
		buf      = M_buf_create();
		M_buf_add_bytes(buf, myval, (size_t)(p-myval));
		M_buf_add_str(buf, p+wlen_last);
		M_free(myval);
		myval    = M_str_trim(M_buf_finish_str(buf, NULL));
		has_last = M_TRUE;
	}

	if (M_str_isempty(myval)) {
		/* If last was used we could have an empty value. In this case
 		 * the offset is the last offset. Otherwise it's an invalid expression. */
		if (has_last) {
			offset = (M_int64)array_len;
		} else {
			M_free(myval);
			return M_FALSE;
		}
	} else {
		/* Remove white space between the sign (if it exists) and the number. */
		if (*myval == '-' || *myval == '+') {
			sign   = *myval;
			*myval = ' ';
			p = myval;
			while (M_chr_isspace(*p)) {
				p++;
			}
			*(p-1) = sign;
			M_str_trim(myval);
		}

		if (M_str_to_int64_ex(myval, val_len, 10, &offset, NULL) != M_STR_INT_SUCCESS) {
			M_free(myval);
			return M_FALSE;
		}

		if (offset < 0) {
			/* Negative means index from the right instead of the left. */
			if ((M_int64)array_len + offset <= 0) {
				M_free(myval);
				return M_FALSE;
			}
			offset = (M_int64)array_len + offset;
		} else if (offset == 0) {
			/* 0 off set is invalid because XPath offsets start at 1. */
			M_free(myval);
			return M_FALSE;
		} else {
			/* We have a positive value and last is present.
 			 * Can't index more than the last item. */
			if (has_last) {
				M_free(myval);
				return M_FALSE;
			}
		}
	}
	M_free(myval);

	/* Set the start and number of positions the expression can match. */
	switch (equality) {
		case M_XML_XPATH_POS_EQUALITY_EQ:
			*out_pos = (size_t)offset;
			break;
		case M_XML_XPATH_POS_EQUALITY_LTE:
			*out_pos = 1;
			*out_max = (size_t)offset;
			break;
		case M_XML_XPATH_POS_EQUALITY_GTE:
			*out_pos = (size_t)offset;
			*out_max = array_len;
			break;
		case M_XML_XPATH_POS_EQUALITY_LT:
			*out_pos = 1;
			*out_max = (size_t)offset-1;
			break;
		case M_XML_XPATH_POS_EQUALITY_GT:
			*out_pos = (size_t)offset+1;
			*out_max = array_len;
			break;
	}

	return M_TRUE;
}

static void M_xml_xpath_search_match_node_tag(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	M_xml_node_t *ptr;
	size_t        num_children;
	size_t        i;

	/* Iterate over children of this branch looking for matches */
	num_children = M_xml_node_num_children(node);
	for (i=0; i<num_children; i++) {
		ptr = M_xml_node_child(node, i);
		if (M_xml_node_type(ptr) != M_XML_NODE_TYPE_ELEMENT)
			continue;

		if (M_xml_xpath_search_tag_eq(ptr, seg, flags)) {
			M_xml_xpath_search(ptr, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
		}

		/* This should NOT be an "else if" to the prior statement as there could legitimately be additional
		 * matches at deeper layers, and we need to search those too */
		if (search_recursive) {
			M_xml_xpath_search(ptr, segments, seg_offset, flags, M_TRUE, matches, num_matches);
		}
	}
}

static void M_xml_xpath_search_match_node_attr_any(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	(void)seg;
	(void)search_recursive;

	if (M_hash_dict_num_keys(M_xml_node_attributes(node)) != 0) {
		M_xml_xpath_search(node, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
	}
}

static void M_xml_xpath_search_match_node_attr_has(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	char *attr;
	
	(void)search_recursive;

	/* Silence coverity. Shouldn't be possible, should get [@] at end*/
	if (M_str_len(seg) < 3)
		return;

	/* Remove [@] from the attribute name we need to match on. */
	attr = M_strdup_max(seg+2, M_str_len(seg)-3);
	if (M_xml_node_attribute(node, attr) != NULL) {
		M_xml_xpath_search(node, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
	}
	M_free(attr);
}

static void M_xml_xpath_search_match_node_attr_val(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	char    **parts;
	M_buf_t  *buf   = NULL;
	char     *attr;
	char     *val;
	char     *out;
	size_t    num_parts = 0;
	size_t    i;
	size_t    start = 0;
	size_t    len;

	(void)search_recursive;

	parts = M_str_explode_str('=', seg, &num_parts);
	if (parts == NULL || num_parts == 0 || M_str_len(parts[0]) < 2) {
		M_str_explode_free(parts, num_parts);
		return;
	}

	/* Get the attribute. */
	attr = M_strdup_max(parts[0]+2, M_str_len(parts[0])-2);

	/* If the attribute doesn't exist in the node then we can't match a value. A value of NULL/"" is
 	 * not the same as the node not being present. */
	if (M_xml_node_attribute(node, attr) == NULL) {
		M_str_explode_free(parts, num_parts);
		M_free(attr);
		return;
	}

	/* Get the attribute value by putting the parts after the separtor '=' together. */
	buf = M_buf_create();
	for (i=1; i<num_parts; i++) {
		if (parts[i] == NULL || *(parts[i]) == '\0') {
			continue;
		}
		M_buf_add_str(buf, parts[i]);
	}
	M_str_explode_free(parts, num_parts);

	/* The value "should" be wrapped in '' and end with ]. We need to remove these extra characters. */
	out = M_buf_finish_str(buf, &len);
	if (*out == '\'' || *out == '"') {
		start++;
		len--;
	}
	while (len > 0 && (out[start+len-1] == '\'' || out[start+len-1] == '"' || out[start+len-1] == ']')) {
		len--;
	}
	val = M_strdup_max(out+start, len);
	M_free(out);

	/* Check if the node's value matches. */
	if (M_str_eq(M_xml_node_attribute(node, attr), val)) {
		M_xml_xpath_search(node, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
	}

	M_free(attr);
	M_free(val);
}

static void M_xml_xpath_search_match_node_pos(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	M_xml_node_t             *parent;
	M_xml_node_t             *ptr;
	char                     *val;
	const char               *last_seg;
	M_xml_xpath_match_type_t  match_type;
	M_xml_node_type_t         node_type;
	size_t                    off_pos;
	size_t                    off_max;
	size_t                    nidx               = 0;
	size_t                    num_children;
	size_t                    num_children_elems = 0;
	size_t                    i;

	(void)search_recursive;

	if (seg_offset == 0)
		return;

	/* We have to have a parent to check if this element is at the given position. */
	parent = M_xml_node_parent(node);
	if (parent == NULL)
		return;

	/* The total number of children in parent. */
	num_children = M_xml_node_num_children(parent);
	if (num_children == 0)
		return;

	/* Get the last segement and verify it's a tag. We need to match based on the tag name. */
	last_seg   = M_list_str_at(segments, seg_offset-1);
	match_type = M_xml_xpath_search_sement_type(last_seg);
	if (match_type != M_XML_XPATH_MATCH_TYPE_TAG && match_type != M_XML_XPATH_MATCH_TYPE_TEXT)
		return;

	/* Determine how many elements of tag name are in the parent. */
	for (i=0; i<num_children; i++) {
		ptr       = M_xml_node_child(parent, i);
		node_type = M_xml_node_type(ptr);
		if ((match_type == M_XML_XPATH_MATCH_TYPE_TAG && node_type == M_XML_NODE_TYPE_ELEMENT && M_xml_xpath_search_tag_eq(ptr, last_seg, flags)) ||
			(match_type == M_XML_XPATH_MATCH_TYPE_TEXT && node_type == M_XML_NODE_TYPE_TEXT))
		{
			num_children_elems++;
		}
	}
	if (num_children_elems == 0)
		return;

	/* Silence coverity. Shouldn't be possible, should always have at least [] */
	if (M_str_len(seg) < 2)
		return;

	/* Strip off []. */
	val = M_strdup_max(seg+1, M_str_len(seg)-2);
	/* Get the position we need to check. */
	if (!M_xml_xpath_search_match_node_pos_offset(val, M_str_len(val), num_children_elems, &off_pos, &off_max)) {
		M_free(val);
		return;
	}
	M_free(val);

	/* Offsets are 1 based. */
	if (off_pos == 0 || off_pos > num_children) {
		return;
	}

	/* We're looking for the index node. Go though all of the nodes in parent until we find this
 	 * node. We'll track it's index to see if it matches the possible indexes from the expression. */
	for (i=0; i<num_children; i++) {
		ptr       = M_xml_node_child(parent, i);
		node_type = M_xml_node_type(ptr);

		/* Skip anything that's not one of the nodes we allow matching on. */
		if (node_type != M_XML_NODE_TYPE_ELEMENT && node_type != M_XML_NODE_TYPE_TEXT)
			continue;

		/* If it's an element and it matches or it's a text node it is considered a possible node. */
		if ((node_type == M_XML_NODE_TYPE_ELEMENT && M_xml_xpath_search_tag_eq(ptr, last_seg, flags)) ||
				(node_type == M_XML_NODE_TYPE_TEXT))
		{
			/* Increment the index. */
			nidx++;
		}

		/* If ptr == node then we've gone as far as we need to. We've found the node we're looking for
 		 * and as far as we're concerned index is index it's at in parent.  */
		if (ptr == node) {
			/* If the index is between the allowed indexes from the expression, be it a single index
 			 * or a range continue processing with this node. */
			if (nidx >= off_pos && nidx < off_pos+off_max) {
				M_xml_xpath_search(ptr, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
			}
			/* Don't need to check later elements because we've found the node we're looking for. */
			break;
		}
	}
}

static void M_xml_xpath_search_match_node_text(const char *seg, M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	M_xml_node_t      *ptr;
	M_xml_node_type_t  type;
	size_t             num_children;
	size_t             i;

	(void)seg;

	num_children = M_xml_node_num_children(node);	
	for (i=0; i<num_children; i++) {
		ptr  = M_xml_node_child(node, i);
		type = M_xml_node_type(ptr);

		if (type == M_XML_NODE_TYPE_TEXT) {
			M_xml_xpath_search(ptr, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
		} else if (search_recursive && type == M_XML_NODE_TYPE_ELEMENT) {
			M_xml_xpath_search(ptr, segments, seg_offset, flags, M_TRUE, matches, num_matches);
		}
	}
}

static void M_xml_xpath_search_add_match(M_xml_node_t *node, M_xml_node_t ***matches, size_t *num_matches)
{
	if (*num_matches == 0 || *matches == NULL || M_size_t_round_up_to_power_of_two(*num_matches) == *num_matches) {
		*matches = M_realloc(*matches, M_size_t_round_up_to_power_of_two(*num_matches + 1) * sizeof(**matches));
	}
	(*matches)[*num_matches] = node;
	(*num_matches)++;
}

static void M_xml_xpath_search(M_xml_node_t *node, M_list_str_t *segments, size_t seg_offset, M_uint32 flags, M_bool search_recursive, M_xml_node_t ***matches, size_t *num_matches)
{
	const char               *seg;
	M_xml_node_type_t         type;
	size_t                    num_segments;
	M_xml_xpath_match_type_t  match_type;

	if (node == NULL)
		return;

	num_segments = M_list_str_len(segments)-seg_offset;
	if (num_segments == 0) {
		M_xml_xpath_search_add_match(node, matches, num_matches);
		return;
	}

	type = M_xml_node_type(node);
	if (type != M_XML_NODE_TYPE_ELEMENT && type != M_XML_NODE_TYPE_DOC && type != M_XML_NODE_TYPE_TEXT)
		return;

	seg = M_list_str_at(segments, seg_offset);
	/* A blank segment denotes we want to search recursively for the next pattern */
	if (seg == NULL || *seg == '\0' || M_str_eq(seg, ".")) {
		/* Only recurse if there is something else to match */
		if (num_segments > 1) {
			M_xml_xpath_search(node, segments, seg_offset+1, flags, M_TRUE, matches, num_matches);
		}
		return;
	}

	/* Are we moving up to the parent? */
	if (M_str_eq(seg, "..")) {
		if (M_xml_node_parent(node) != NULL) {
			node = M_xml_node_parent(node);
		}
		M_xml_xpath_search(node, segments, seg_offset+1, flags, M_FALSE, matches, num_matches);
		return;
	}

	/* Determine they type of match we need to use. */
	match_type = M_xml_xpath_search_sement_type(seg);
	switch (match_type) {
		case M_XML_XPATH_MATCH_TYPE_TAG:
			M_xml_xpath_search_match_node_tag(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_ATTR_ANY:
			M_xml_xpath_search_match_node_attr_any(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_ATTR_HAS:
			M_xml_xpath_search_match_node_attr_has(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_ATTR_VAL:
			M_xml_xpath_search_match_node_attr_val(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_POS:
			M_xml_xpath_search_match_node_pos(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_TEXT:
			M_xml_xpath_search_match_node_text(seg, node, segments, seg_offset, flags, search_recursive, matches, num_matches);
			break;
		case M_XML_XPATH_MATCH_TYPE_INVALID:
			break;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_xml_node_t **M_xml_xpath(M_xml_node_t *node, const char *search, M_uint32 flags, size_t *num_matches)
{
	M_xml_node_t **matches            = NULL;
	char         **segments;
	char          **pred_segments;
	M_list_str_t   *seg_list;
	M_buf_t        *buf;
	char           *out;
	size_t          start_offset      = 0;
	size_t          num_segments      = 0;
	size_t          num_pred_segments = 0;
	size_t          i;
	size_t          j;

	if (num_matches == NULL) {
		return NULL;
	}
	*num_matches = 0;

	if (node == NULL || search == NULL)
		return NULL;

	segments = M_str_explode_str('/', search, &num_segments);
	if (segments == NULL || num_segments == 0) {
		/* Silence coverity, but most likely if num_segments is 0, segments is NULL right? */
		M_str_explode_free(segments, num_segments);
		return NULL;
	}

	/* Further split on '[' to pull out predicate filters. */
	seg_list = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<num_segments; i++) {
		if (*(segments[i]) == '\0') {
			M_list_str_insert(seg_list, "");
			continue;
		}

		pred_segments = M_str_explode_str('[', segments[i], &num_pred_segments);
		if (pred_segments == NULL || num_pred_segments == 0) {
			M_str_explode_free(pred_segments, num_pred_segments);
			continue;
		}

		for (j=0; j<num_pred_segments; j++) {
			/* Empty means we found a '[', skip it. */
			if (pred_segments[j] == NULL || *(pred_segments[j]) == '\0')
				continue;

			/* First one may not start with '['. We need to check if the segement is something like:
			 * 'abc'/'abc[1]' vs '[1]'. */
			if (j == 0 && *(segments[i]) != '[') {
				M_list_str_insert(seg_list, pred_segments[j]);
				continue;
			}
			
			/* Verify that our predicate ends with a ']'. If it doesn't then this is an invaild expression. */
			if (pred_segments[j][M_str_len(pred_segments[j])-1] != ']') {
				M_str_explode_free(pred_segments, num_pred_segments);
				M_str_explode_free(segments, num_segments);
				M_list_str_destroy(seg_list);
				return NULL;
			}

			/* Put the '[' back on the front of the segement and add it to our list of segements. */
			buf = M_buf_create();
			M_buf_add_byte(buf, '[');
			M_buf_add_str(buf, pred_segments[j]);
			out = M_buf_finish_str(buf, NULL);
			M_list_str_insert(seg_list, out);
			M_free(out);
		}
		M_str_explode_free(pred_segments, num_pred_segments);
	}
	M_str_explode_free(segments, num_segments);

	if (M_list_str_len(seg_list) == 0) {
		M_list_str_destroy(seg_list);
		return NULL;
	}

	if (M_str_len(M_list_str_at(seg_list, 0)) == 0) {
		/* If the first node is blank, that means the search pattern started with '/',
		 * which means we need to scan to the doc node */
		node         = M_xml_node_find_doc(node);
		start_offset = 1;
	} else {
		/* Anything else is the start of the search pattern, so call
		 * the actual search function */
	}

	if (M_list_str_len(seg_list) - start_offset != 0) {
		/* Only do the search if there is something to search */
		M_xml_xpath_search(node, seg_list, start_offset, flags, M_FALSE, &matches, num_matches);
	} else {
		/* Otherwise I suppose it makes sense to return the current node */
		matches      = M_malloc(sizeof(*matches));
		matches[0]   = node;
		*num_matches = 1;
	}

	M_list_str_destroy(seg_list);
	return matches;
}

const char *M_xml_xpath_text_first(M_xml_node_t *node, const char *search)
{
	M_xml_node_t **matches     = NULL;
	M_xml_node_t  *match;
	const char    *text        = NULL;
	size_t         num_matches = 0;
	size_t         num_children;
	size_t         i;

	matches = M_xml_xpath(node, search, M_XML_READER_NONE, &num_matches);
	if (matches != NULL && num_matches > 0) {
		match = matches[0];

		/* Check if we got a text node. (expression with /text()) */
		if (M_xml_node_type(match) == M_XML_NODE_TYPE_TEXT) {
			text = M_xml_node_text(match);
		}

		/* Otherwise we have an element and we want to pull the first text node if it has one. */
		num_children = M_xml_node_num_children(match);
		for (i=0; i<num_children; i++) {
			if (M_xml_node_type(M_xml_node_child(match, i)) == M_XML_NODE_TYPE_TEXT) {
				text = M_xml_node_text(M_xml_node_child(match, i));
				break;
			}
		}
	}

	M_free(matches);

	/* The xpath succeeded, but there was no actual text.  Return blank text
	 * rather than NULL to indicate that the node requested did exist, just
	 * there was no text element in it. */
	if (text == NULL && num_matches > 0) {
		text = "";
	}
	return text;
}
