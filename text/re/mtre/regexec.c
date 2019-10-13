/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

/*
   Copyright © 2005-2019 Rich Felker, et al.
   
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
   
   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
   regexec.c - TRE POSIX compatible matching functions (and more).

   Copyright (c) 2001-2009 Ville Laurikari <vl@iki.fi>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "tre.h"

static void tre_fill_pmatch(size_t nmatch, regmatch_t pmatch[],
		const tre_tnfa_t *tnfa, regoff_t *tags, regoff_t match_eo);

/***********************************************************************
  from tre-match-utils.h
 ***********************************************************************/

#define GET_NEXT_WCHAR() do {                                                 \
	prev_c = next_c; pos += pos_add_next;                                     \
	if (*str_byte == '\0') { \
		next_c = '\0'; \
		pos_add_next++; \
	} else { \
		if (M_utf8_get_cp(str_byte, &next_c, &next_s) != M_UTF8_ERROR_SUCCESS) { \
			ret = REG_NOMATCH; goto error_exit; \
		} \
		pos_add_next = next_s - str_byte; \
	} \
	str_byte += pos_add_next;                                                 \
} while (0)

#define IS_WORD_CHAR(c) ((c) == '_' || M_utf8_isalnum_cp(c))

#define CHECK_ASSERTIONS(assertions)     \
	(((assertions & ASSERT_AT_BOL)     \
	  && (pos > 0)     \
	  && (prev_c != '\n' || !reg_multiline))     \
	  || ((assertions & ASSERT_AT_EOL)     \
		  && (next_c != '\0')     \
		  && (next_c != '\n' || !reg_multiline))     \
		  || ((assertions & ASSERT_AT_BOW)     \
			  && (IS_WORD_CHAR(prev_c) || !IS_WORD_CHAR(next_c)))             \
			  || ((assertions & ASSERT_AT_EOW)     \
				  && (!IS_WORD_CHAR(prev_c) || IS_WORD_CHAR(next_c)))     \
				  || ((assertions & ASSERT_AT_WB)     \
					  && (pos != 0 && next_c != '\0'     \
						  && IS_WORD_CHAR(prev_c) == IS_WORD_CHAR(next_c)))     \
						  || ((assertions & ASSERT_AT_WB_NEG)     \
							  && (pos == 0 || next_c == '\0'     \
								  || IS_WORD_CHAR(prev_c) != IS_WORD_CHAR(next_c))))


/* Returns 1 if `t1' wins `t2', 0 otherwise. */
static int tre_tag_order(int num_tags, tre_tag_direction_t *tag_directions, regoff_t *t1, regoff_t *t2)
{
	int i;

	for (i = 0; i < num_tags; i++) {
		if (tag_directions[i] == TRE_TAG_MINIMIZE || tag_directions[i] == TRE_TAG_LEFT_MAXIMIZE) {
			if (t1[i] < t2[i]) {
				return 1;
			}

			if (t1[i] > t2[i]) {
				return 0;
			}
		} else {
			if (t1[i] > t2[i]) {
				return 1;
			}

			if (t1[i] < t2[i]) {
				return 0;
			}
		}
	}

	return 0;
}


/***********************************************************************
  from tre-match-parallel.c
 ***********************************************************************/

/*
   This algorithm searches for matches basically by reading characters
   in the searched string one by one, starting at the beginning. All
   matching paths in the TNFA are traversed in parallel. When two or
   more paths reach the same state, exactly one is chosen according to
   tag ordering rules; if returning submatches is not required it does
   not matter which path is chosen.

   The worst case time required for finding the leftmost and longest
   match, or determining that there is no match, is always linearly
   dependent on the length of the text being searched.

   This algorithm cannot handle TNFAs with back referencing nodes.
   See `tre-match-backtrack.c'.
   */

typedef struct {
	tre_tnfa_transition_t *state;
	regoff_t              *tags;
} tre_tnfa_reach_t;

typedef struct {
	regoff_t   pos;
	regoff_t **tags;
} tre_reach_pos_t;


static reg_errcode_t tre_tnfa_run_parallel(const tre_tnfa_t *tnfa, const void *string,
		regoff_t *match_tags, regoff_t *match_end_ofs)
{
	/* State variables required by GET_NEXT_WCHAR. */
	M_uint32       prev_c        = 0;
	M_uint32       next_c        = 0;
	const char    *next_s        = NULL;
	const char    *str_byte      = string;
	regoff_t       pos           = -1;
	regoff_t       pos_add_next  = 1;
	int            reg_multiline = tnfa->cflags & REG_MULTILINE;
	reg_errcode_t  ret;

	char                  *buf;
	tre_tnfa_transition_t *trans_i;
	tre_tnfa_reach_t      *reach;
	tre_tnfa_reach_t      *reach_next;
	tre_tnfa_reach_t      *reach_i;
	tre_tnfa_reach_t      *reach_next_i;
	tre_reach_pos_t       *reach_pos;
	int                   *tag_i;
	int                    num_tags;
	int                    i;

	regoff_t  match_eo  = -1;  /* end offset of match (-1 if no match found yet) */
	int       new_match = 0;
	regoff_t *tmp_tags  = NULL;
	regoff_t *tmp_iptr;

	if (!match_tags) {
		num_tags = 0;
	} else {
		num_tags = tnfa->num_tags;
	}

	/* Allocate memory for temporary data required for matching. This needs to
	   be done for every matching operation to be thread safe.  This allocates
	   everything in a single large block with calloc(). */
	{
		size_t  tbytes;
		size_t  rbytes;
		size_t  pbytes;
		size_t  xbytes;
		size_t  total_bytes;
		char   *tmp_buf;

		/* Ensure that tbytes and xbytes*num_states cannot overflow, and that
		 * they don't contribute more than 1/8 of SIZE_MAX to total_bytes. */
		if ((size_t)num_tags > SIZE_MAX/(8 * sizeof(regoff_t) * (size_t)tnfa->num_states))
			return REG_ESPACE;

		/* Likewise check rbytes. */
		if ((size_t)tnfa->num_states+1 > SIZE_MAX/(8 * sizeof(*reach_next)))
			return REG_ESPACE;

		/* Likewise check pbytes. */
		if ((size_t)tnfa->num_states > SIZE_MAX/(8 * sizeof(*reach_pos)))
			return REG_ESPACE;

		/* Compute the length of the block we need. */
		tbytes = sizeof(*tmp_tags) * (size_t)num_tags;
		rbytes = sizeof(*reach_next) * (size_t)(tnfa->num_states + 1);
		pbytes = sizeof(*reach_pos) * (size_t)tnfa->num_states;
		xbytes = sizeof(regoff_t) * (size_t)num_tags;
		total_bytes =
			(sizeof(long) - 1) * 4 /* for alignment paddings */
			+ (rbytes + xbytes * (size_t)tnfa->num_states) * 2 + tbytes + pbytes;

		/* Allocate the memory. */
		buf = M_malloc_zero(total_bytes);

		/* Get the various pointers within tmp_buf (properly aligned). */
		tmp_tags   = (void *)buf;
		tmp_buf    = buf + tbytes;
		tmp_buf   += ALIGN(tmp_buf, long);
		reach_next = (void *)tmp_buf;
		tmp_buf   += rbytes;
		tmp_buf   += ALIGN(tmp_buf, long);
		reach      = (void *)tmp_buf;
		tmp_buf   += rbytes;
		tmp_buf   += ALIGN(tmp_buf, long);
		reach_pos  = (void *)tmp_buf;
		tmp_buf   += pbytes;
		tmp_buf   += ALIGN(tmp_buf, long);

		for (i = 0; i < tnfa->num_states; i++) {
			reach[i].tags      = (void *)tmp_buf;
			tmp_buf           += xbytes;
			reach_next[i].tags = (void *)tmp_buf;
			tmp_buf           += xbytes;
		}
	}

	for (i = 0; i < tnfa->num_states; i++) {
		reach_pos[i].pos = -1;
	}

	GET_NEXT_WCHAR();
	pos = 0;

	reach_next_i = reach_next;
	while (1) {
		/* If no match found yet, add the initial states to `reach_next'. */
		if (match_eo < 0) {
			trans_i = tnfa->initial;
			while (trans_i->state != NULL) {
				if (reach_pos[trans_i->state_id].pos < pos) {
					if (trans_i->assertions && CHECK_ASSERTIONS(trans_i->assertions)) {
						trans_i++;
						continue;
					}

					reach_next_i->state = trans_i->state;
					for (i = 0; i < num_tags; i++) {
						reach_next_i->tags[i] = -1;
					}

					tag_i = trans_i->tags;
					if (tag_i) {
						while (*tag_i >= 0) {
							if (*tag_i < num_tags) {
								reach_next_i->tags[*tag_i] = pos;
							}
							tag_i++;
						}
					}

					if (reach_next_i->state == tnfa->final_trans) {
						match_eo  = pos;
						new_match = 1;
						for (i = 0; i < num_tags; i++) {
							match_tags[i] = reach_next_i->tags[i];
						}
					}

					reach_pos[trans_i->state_id].pos  = pos;
					reach_pos[trans_i->state_id].tags = &reach_next_i->tags;
					reach_next_i++;
				}

				trans_i++;
			}

			reach_next_i->state = NULL;
		} else {
			if (num_tags == 0 || reach_next_i == reach_next) {
				/* We have found a match. */
				break;
			}
		}

		/* Check for end of string. */
		if (!next_c) {
			break;
		}

		GET_NEXT_WCHAR();

		/* Swap `reach' and `reach_next'. */
		reach_i    = reach;
		reach      = reach_next;
		reach_next = reach_i;

		/* For each state in `reach', weed out states that don't fulfill the
		   minimal matching conditions. */
		if (tnfa->num_minimals && new_match) {
			new_match    = 0;
			reach_next_i = reach_next;

			for (reach_i = reach; reach_i->state; reach_i++) {
				M_bool skip = M_FALSE;

				for (i = 0; tnfa->minimal_tags[i] >= 0; i += 2) {
					int end   = tnfa->minimal_tags[i];
					int start = tnfa->minimal_tags[i+1];

					if (end >= num_tags) {
						skip = M_TRUE;
						break;
					} else if (reach_i->tags[start] == match_tags[start] && reach_i->tags[end] < match_tags[end]) {
						skip = M_TRUE;
						break;
					}
				}

				if (!skip) {
					reach_next_i->state = reach_i->state;
					tmp_iptr            = reach_next_i->tags;
					reach_next_i->tags  = reach_i->tags;
					reach_i->tags       = tmp_iptr;
					reach_next_i++;
				}
			}

			reach_next_i->state = NULL;

			/* Swap `reach' and `reach_next'. */
			reach_i    = reach;
			reach      = reach_next;
			reach_next = reach_i;
		}

		/* For each state in `reach' see if there is a transition leaving with
		   the current input symbol to a state not yet in `reach_next', and
		   add the destination states to `reach_next'. */
		reach_next_i = reach_next;
		for (reach_i = reach; reach_i->state; reach_i++) {
			for (trans_i = reach_i->state; trans_i->state; trans_i++) {
				/* Does this transition match the input symbol? */
				if (trans_i->code_min <= (int)prev_c && trans_i->code_max >= (int)prev_c) {
					if (trans_i->assertions && CHECK_ASSERTIONS(trans_i->assertions)) {
						continue;
					}

					/* Compute the tags after this transition. */
					for (i = 0; i < num_tags; i++) {
						tmp_tags[i] = reach_i->tags[i];
					}

					tag_i = trans_i->tags;
					if (tag_i != NULL) {
						while (*tag_i >= 0) {
							if (*tag_i < num_tags) {
								tmp_tags[*tag_i] = pos;
							}
							tag_i++;
						}
					}

					if (reach_pos[trans_i->state_id].pos < pos) {
						/* Found an unvisited node. */
						reach_next_i->state               = trans_i->state;
						tmp_iptr                          = reach_next_i->tags;
						reach_next_i->tags                = tmp_tags;
						tmp_tags                          = tmp_iptr;
						reach_pos[trans_i->state_id].pos  = pos;
						reach_pos[trans_i->state_id].tags = &reach_next_i->tags;

						if (reach_next_i->state == tnfa->final_trans
								&& (match_eo == -1 || (num_tags > 0 && reach_next_i->tags[0] <= match_tags[0])))
						{
							match_eo  = pos;
							new_match = 1;

							for (i = 0; i < num_tags; i++) {
								match_tags[i] = reach_next_i->tags[i];
							}
						}

						reach_next_i++;
					} else {
						/* Another path has also reached this state.  We choose
						   the winner by examining the tag values for both
						   paths. */
						if (tre_tag_order(num_tags, tnfa->tag_directions,
									tmp_tags, *reach_pos[trans_i->state_id].tags))
						{
							/* The new path wins. */
							tmp_iptr                           = *reach_pos[trans_i->state_id].tags;
							*reach_pos[trans_i->state_id].tags = tmp_tags;
							if (trans_i->state == tnfa->final_trans) {
								match_eo  = pos;
								new_match = 1;

								for (i = 0; i < num_tags; i++) {
									match_tags[i] = tmp_tags[i];
								}
							}

							tmp_tags = tmp_iptr;
						}
					}
				}
			}
		}

		reach_next_i->state = NULL;
	}

	*match_end_ofs = match_eo;
	ret = match_eo >= 0 ? REG_OK : REG_NOMATCH;

error_exit:
	M_free(buf);
	return ret;
}

/***********************************************************************
  from regexec.c
 ***********************************************************************/

/* Fills the POSIX.2 regmatch_t array according to the TNFA tag and match
   endpoint values. */
static void tre_fill_pmatch(size_t nmatch, regmatch_t pmatch[],
		const tre_tnfa_t *tnfa, regoff_t *tags, regoff_t match_eo)
{
	const tre_submatch_data_t *submatch_data;
	unsigned int               i;
	unsigned int               j;
	int                       *parents;

	if (pmatch == NULL)
		return;

	i = 0;
	if (match_eo >= 0) {
		/* Construct submatch offsets from the tags. */
		submatch_data = tnfa->submatch_data;

		while (i < tnfa->num_submatches && i < nmatch) {
			if (submatch_data[i].so_tag == tnfa->end_tag) {
				pmatch[i].rm_so = match_eo;
			} else {
				pmatch[i].rm_so = tags[submatch_data[i].so_tag];
			}

			if (submatch_data[i].eo_tag == tnfa->end_tag) {
				pmatch[i].rm_eo = match_eo;
			} else {
				pmatch[i].rm_eo = tags[submatch_data[i].eo_tag];
			}

			/* If either of the endpoints were not used, this submatch
			   was not part of the match. */
			if (pmatch[i].rm_so == -1 || pmatch[i].rm_eo == -1) {
				pmatch[i].rm_so = pmatch[i].rm_eo = -1;
			}

			i++;
		}

		/* Reset all submatches that are not within all of their parent submatches. */
		i = 0;
		while (i < tnfa->num_submatches && i < nmatch) {
			parents = submatch_data[i].parents;
			if (parents != NULL) {
				for (j = 0; parents[j] >= 0; j++) {
					if (pmatch[i].rm_so < pmatch[parents[j]].rm_so || pmatch[i].rm_eo > pmatch[parents[j]].rm_eo) {
						pmatch[i].rm_so = pmatch[i].rm_eo = -1;
					}
				}
			}

			i++;
		}
	}

	while (i < nmatch) {
		pmatch[i].rm_so = -1;
		pmatch[i].rm_eo = -1;
		i++;
	}
}


reg_errcode_t mregexec(const regex_t *restrict preg, const char *restrict string,
		size_t nmatch, regmatch_t pmatch[restrict])
{
	tre_tnfa_t    *tnfa = (void *)preg->tnfa;
	reg_errcode_t  status;
	regoff_t      *tags = NULL;
	regoff_t       eo;

	if (tnfa->num_tags > 0 && nmatch > 0)
		tags = M_malloc(sizeof(*tags) * (size_t)tnfa->num_tags);

	/* Parallel matcher. */
	status = tre_tnfa_run_parallel(tnfa, string, tags, &eo);

	/* A match was found, so fill the submatch registers. */
	if (status == REG_OK)
		tre_fill_pmatch(nmatch, pmatch, tnfa, tags, eo);

	if (tags)
		M_free(tags);

	return status;
}
