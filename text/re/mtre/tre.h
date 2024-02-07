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
   tre-internal.h - TRE internal definitions

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

#ifndef __TRE_H__
#define __TRE_H__

#include <mstdlib/mstdlib.h>

#include "mregex.h"

#define TRE_CHAR_MAX 0x10ffff

/* Returns number of bytes to add to (char *)ptr to make it
   properly aligned for the type. */
#define ALIGN(ptr, type) \
    ((((M_uintptr)ptr) % sizeof(type)) \
     ? (sizeof(type) - (((M_uintptr)ptr) % sizeof(type))) \
     : 0)

/* Assertions. */
typedef enum {
    ASSERT_AT_BOL         = 1 << 0, /*!< Beginning of line. */
    ASSERT_AT_EOL         = 1 << 1, /*!< End of line. */
    ASSERT_AT_BOW         = 1 << 4, /*!< Beginning of word. */
    ASSERT_AT_EOW         = 1 << 5, /*!< End of word. */
    ASSERT_AT_WB          = 1 << 6, /*!< Word boundary. */
    ASSERT_AT_WB_NEG      = 1 << 7, /*!< Not a word boundary. */
    ASSERT_LAST           = 1 << 8
} tre_assert_t;

/* TNFA transition type. A TNFA state is an array of transitions,
   the terminator is a transition with NULL `state'. */
typedef struct tnfa_transition tre_tnfa_transition_t;

struct tnfa_transition {
    /* Range of accepted characters. */
    int                    code_min;
    int                    code_max;
    /* Pointer to the destination state. */
    tre_tnfa_transition_t *state;
    /* ID number of the destination state. */
    int                    state_id;
    /* -1 terminated array of tags (or NULL). */
    int                   *tags;
    /* Assertion bitmap. */
    tre_assert_t           assertions;
};

/* Tag directions. */
typedef enum {
    TRE_TAG_MINIMIZE = 0,
    TRE_TAG_MAXIMIZE,
    TRE_TAG_LEFT_MAXIMIZE
} tre_tag_direction_t;

/* Instructions to compute submatch register values from tag values
   after a successful match.  */
struct tre_submatch_data {
    /* Tag that gives the value for rm_so (submatch start offset). */
    int  so_tag;
    /* Tag that gives the value for rm_eo (submatch end offset). */
    int  eo_tag;
    /* List of submatches this submatch is contained in. */
    int *parents;
};

typedef struct tre_submatch_data tre_submatch_data_t;


/* TNFA definition. */
typedef struct tnfa tre_tnfa_t;

struct tnfa {
    tre_tnfa_transition_t *transitions;
    unsigned int           num_transitions;
    tre_tnfa_transition_t *initial;
    tre_tnfa_transition_t *final_trans;
    tre_submatch_data_t   *submatch_data;
    char                  *firstpos_chars;
    int                    first_char;
    unsigned int           num_submatches;
    tre_tag_direction_t  *tag_directions;
    int                  *minimal_tags;
    int                   num_tags;
    int                   num_minimals;
    int                   end_tag;
    int                   num_states;
    regex_flags_t         cflags;
};

/* from tre-mem.h: */

#define TRE_MEM_BLOCK_SIZE 1024

typedef struct tre_list {
    void            *data;
    struct tre_list *next;
} tre_list_t;

typedef struct tre_mem_struct {
    tre_list_t  *blocks;
    tre_list_t  *current;
    char        *ptr;
    size_t       n;
    void       **provided;
} *tre_mem_t;

/* Returns a new memory allocator or NULL if out of memory. */
tre_mem_t tre_mem_new(void);

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block or NULL if an underlying malloc() failed. */
void *tre_mem_alloc(tre_mem_t mem, size_t size);

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block or NULL if an underlying malloc() failed.  The memory
   is set to zero. */
void *tre_mem_calloc(tre_mem_t mem, size_t size);

/* Frees the memory allocator and all memory allocated with it. */
void tre_mem_destroy(tre_mem_t mem);

#endif /* __TRE_H__ */
