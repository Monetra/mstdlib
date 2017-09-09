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
#include "json/m_json_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Turn a string into an index. This will adjust negative numbers to count from the end of the array.
 * On success out will never be < 0. Out can be >= array_len.
 */
static M_bool M_json_jsonpath_search_array_offset_val(const char *val, size_t val_len, size_t array_len, M_uint32 *out)
{
	M_int32 offset;

	*out = 0;
	if (M_str_to_int32_ex(val, val_len, 10, &offset, NULL) != M_STR_INT_SUCCESS) {
		return M_FALSE;
	}

	if (offset < 0) {
		if ((M_int32)array_len + offset < 0) {
			return M_FALSE;
		}
		*out = (M_uint32)((M_int32)array_len + offset);
	} else {
		*out = (M_uint32)offset;
	}

	return M_TRUE;
}

static M_list_u64_t *M_json_jsonpath_search_array_offsets(const M_json_node_t *node, const char *segment)
{
	M_list_u64_t  *offsets;
	size_t         seg_len;
	size_t         array_len;
	char         **comma_parts      = NULL;
	size_t        *comma_parts_lens = NULL;
	size_t         comma_num_parts  = 0;
	char         **slice_parts      = NULL;
	size_t        *slice_parts_lens = NULL;
	size_t         slice_num_parts  = 0;
	size_t         i;
	M_int64        j;
	M_uint32       slice_start;
	M_uint32       slice_end;
	M_int32        slice_step;

	if (M_json_node_type(node) != M_JSON_TYPE_ARRAY)
		return NULL;

	array_len = M_json_array_len(node);
	seg_len   = M_str_len(segment);

	/* If we don't have any items in this node there is nothing to index. Or if there isn't any data between
 	 * '[' and ']' then we don't have an offset to index. */
	if (array_len == 0 || seg_len < 3)
		return NULL;

	/* The list is not sorted so we can have duplicates and out of order results can be returned, "[2,1]" */
	offsets = M_list_u64_create(M_LIST_U64_NONE);

	/* We're going to first explode on ',' and go though each of our indexes. If there isn't a ',' then
 	 * the first and only element expoded with be the value we want to deal with. */
	comma_parts = M_str_explode(',', segment+1, seg_len-2, &comma_num_parts, &comma_parts_lens);
	if (comma_parts == NULL || comma_num_parts == 0)
		goto error;

	/* Go thoguh each of the elements we expoded with ',' */
	for (i=0; i<comma_num_parts; i++) {
		/* we're going to explode on ':' and look for slices. If this isn't a slice and we have a single index
 		 * the value will be the first and only element exploded. */
		slice_parts = M_str_explode(':', comma_parts[i], comma_parts_lens[i], &slice_num_parts, &slice_parts_lens);
		if (slice_parts == NULL || slice_num_parts == 0 || slice_num_parts > 3) {
			goto slice_done;
		}

		/* If we have a slice we're going to set some defaults. It's allowed to omit the start, end and step. */
		slice_start = 0;
		slice_end   = (M_uint32)array_len;
		slice_step  = 1;

		/* If we have 2 or more parts we have a slice. */
		if (slice_num_parts == 2 || slice_num_parts == 3) {
			/* Start. */
			if (slice_parts_lens[0] > 0) {
				if (!M_json_jsonpath_search_array_offset_val(slice_parts[0], slice_parts_lens[0], array_len, &slice_start)) {
					goto slice_done;
				}
			}
			/* End. */
			if (slice_parts_lens[1] > 0) {
				if (!M_json_jsonpath_search_array_offset_val(slice_parts[1], slice_parts_lens[1], array_len, &slice_end)) {
					goto slice_done;
				}
			}
			/* Step. */
			if (slice_num_parts == 3 && slice_parts_lens[2] > 0) {
				if (M_str_to_int32_ex(slice_parts[2], slice_parts_lens[2], 10, &slice_step, NULL) != M_STR_INT_SUCCESS || slice_step == 0) {
					goto slice_done;
				}
			}

			/* Cases where we won't calculate anything. */
			if ((slice_start == slice_end) ||
				(slice_start > slice_end && slice_step > 0) ||
				(slice_start < slice_end && slice_step < 0))
			{
				goto slice_done;
			}

			/* Determine the values for each index we want to look at. */
			if (slice_start < slice_end) {
				/* Count up. */
				for (j=slice_start; j<slice_end; j+=slice_step) {
					if (j >= 0 && j < (M_int64)array_len) {
						M_list_u64_insert(offsets, (M_uint64)j);
					}
				}
			} else {
				/* Count down. */
				for (j=slice_start-1; j>=slice_end; j+=slice_step) {
					if (j >= 0 && j < (M_int64)array_len) {
						M_list_u64_insert(offsets, (M_uint64)j);
					}
				}
			}
		} else {
			/* 1 exact index. */
			if (!M_json_jsonpath_search_array_offset_val(slice_parts[0], slice_parts_lens[0], array_len, &slice_start)) {
				goto error;
			}
			/* Check that we have a valid index. */
			if (slice_start < array_len) {
				M_list_u64_insert(offsets, slice_start);
			}
		}

slice_done:
		M_str_explode_free(slice_parts, slice_num_parts);
		M_free(slice_parts_lens);
	}

	M_str_explode_free(comma_parts, comma_num_parts);
	M_free(comma_parts_lens);

	return offsets;

error:
	M_str_explode_free(slice_parts, slice_num_parts);
	M_free(slice_parts_lens);
	M_str_explode_free(comma_parts, comma_num_parts);
	M_free(comma_parts_lens);
	M_list_u64_destroy(offsets);
	return NULL;

}

static void M_json_jsonpath_search_add_match(const M_json_node_t *node, M_json_node_t ***matches, size_t *num_matches)
{
	if (*num_matches == 0 || *matches == NULL || M_size_t_round_up_to_power_of_two(*num_matches) == *num_matches) {
		*matches = M_realloc(*matches, M_size_t_round_up_to_power_of_two(*num_matches + 1) * sizeof(**matches));
	}
	(*matches)[*num_matches] = M_CAST_OFF_CONST(M_json_node_t *, node);
	(*num_matches)++;
}

static void M_json_jsonpath_search(const M_json_node_t *node, M_list_str_t *segments, size_t seg_offset, M_bool search_recursive, M_json_node_t ***matches, size_t *num_matches)
{
	M_hash_strvp_enum_t *hashenum;
	M_list_u64_t        *array_offsets;
	const char          *seg;
	const char          *key;
	void                *val;
	size_t               num_segments;
	size_t               array_len;
	size_t               array_offsets_len;
	size_t               i;

	if (node == NULL)
		return;

	num_segments = M_list_str_len(segments)-seg_offset;
	if (num_segments == 0) {
		M_json_jsonpath_search_add_match(node, matches, num_matches);
		return;
	}

	/* Only objects and arrays can have things under them. */
	if (node->type != M_JSON_TYPE_OBJECT && node->type != M_JSON_TYPE_ARRAY)
		return;

	seg = M_list_str_at(segments, seg_offset);
	/* A blank segment denotes we want to search recursively for the next pattern */
	if (seg == NULL || *seg == '\0') {
		/* Only recurse if there is something else to match */
		if (num_segments > 1) {
			M_json_jsonpath_search(node, segments, seg_offset+1, M_TRUE, matches, num_matches);
		}
		return;
	}

	if (node->type == M_JSON_TYPE_OBJECT) {
		/* Invalid search. We can't index an object. */
		if (*seg == '[')
			return;

		M_hash_strvp_enumerate(node->data.json_object, &hashenum);
		while (M_hash_strvp_enumerate_next(node->data.json_object, hashenum, &key, &val)) {
			/* If a wildcard match, or an exact name match, its a match */
			if (M_str_eq(seg, "*") || M_str_caseeq(seg, key)) {
				M_json_jsonpath_search(val, segments, seg_offset+1, M_FALSE, matches, num_matches);
			}

			/* This should NOT be an "else if" to the prior statement as there could legitimately be additional
			 * matches at deeper layers, and we need to search those too */
			if (search_recursive) {
				M_json_jsonpath_search(val, segments, seg_offset, M_TRUE, matches, num_matches);
			}
		}
		M_hash_strvp_enumerate_free(hashenum);
	} else if (node->type == M_JSON_TYPE_ARRAY) {
		array_len = M_json_array_len(node);
		if (M_str_eq(seg, "[*]")) {
			for (i=0; i<array_len; i++) {
				M_json_jsonpath_search(M_json_array_at(node, i), segments, seg_offset+1, search_recursive, matches, num_matches);
			}
		/* We have an indexed value lets try to find that index. */
		} else if (*seg == '[') {
			array_offsets     = M_json_jsonpath_search_array_offsets(node, seg);
			array_offsets_len = M_list_u64_len(array_offsets);
			for (i=0; i<array_offsets_len; i++) {
				M_json_jsonpath_search(M_json_array_at(node, (size_t)M_list_u64_at(array_offsets, i)), segments, seg_offset+1, M_FALSE, matches, num_matches);
			}
			M_list_u64_destroy(array_offsets);
		}

		/* This should NOT be an "else if" to the prior statement as there could legitimately be additional
		 * matches at deeper layers, and we need to search those too */
		if (search_recursive) {
			for (i=0; i<array_len; i++) {
				M_json_jsonpath_search(M_json_array_at(node, i), segments, seg_offset, M_TRUE, matches, num_matches);
			}
		}
	}
}

M_json_node_t **M_json_jsonpath(const M_json_node_t *node, const char *search, size_t *num_matches)
{
	M_json_node_t **matches          = NULL;
	char          **segments;
	char          **idx_segments;
	M_list_str_t   *seg_list;
	M_buf_t        *buf;
	char           *out;
	size_t          num_segments     = 0;
	size_t          num_idx_segments = 0;
	size_t          i;
	size_t          j;

	if (node == NULL || search == NULL || num_matches == NULL)
		return NULL;

	*num_matches = 0;

	/* All JSON search expressions must start with a '$'. */
	if (M_str_len(search) < 1 || *search != '$')
		return NULL;

	segments = M_str_explode_str('.', search+1, &num_segments);
	if (segments == NULL || num_segments == 0) {
		/* Silence coverity, if num_segments is 0, segments should be NULL */
		M_str_explode_free(segments, num_segments);
		return NULL;
	}

	/* Further split on '[' to pull out indexes */
	seg_list = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<num_segments; i++) {
		if (*(segments[i]) == '\0') {
			M_list_str_insert(seg_list, "");
			continue;
		}

		idx_segments = M_str_explode_str('[', segments[i], &num_idx_segments);
		if (idx_segments == NULL || num_idx_segments == 0) {
			M_str_explode_free(idx_segments, num_idx_segments);
			continue;
		}

		for (j=0; j<num_idx_segments; j++) {
			/* Empty means we found a '[', skip it. */
			if (idx_segments[j] == NULL || *(idx_segments[j]) == '\0')
				continue;

			/* First one may not start with '['. We need to check if the segement is something like:
			 * 'abc'/'abc[1]' vs '[1]'. */
			if (j == 0 && *(segments[i]) != '[') {
				M_list_str_insert(seg_list, idx_segments[j]);
				continue;
			}

			/* Put the '[' back on the front of the segement and add it to our list of segements. */
			buf = M_buf_create();
			M_buf_add_byte(buf, '[');
			M_buf_add_str(buf, idx_segments[j]);
			out = M_buf_finish_str(buf, NULL);
			M_list_str_insert(seg_list, out);
			M_free(out);
		}
		M_str_explode_free(idx_segments, num_idx_segments);
	}
	M_str_explode_free(segments, num_segments);

	M_json_jsonpath_search(node, seg_list, 0, M_FALSE, &matches, num_matches);
	M_list_str_destroy(seg_list);
	return matches;
}
