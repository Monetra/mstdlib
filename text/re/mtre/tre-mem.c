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
   tre-mem.c - TRE memory allocator

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

/*
   This memory allocator is for allocating small memory blocks efficiently
   in terms of memory overhead and execution speed.  The allocated blocks
   cannot be freed individually, only all at once.  There can be multiple
   allocators, though.
   */

#include <stdlib.h>
#include <string.h>

#include "tre.h"

/*
   This memory allocator is for allocating small memory blocks efficiently
   in terms of memory overhead and execution speed.  The allocated blocks
   cannot be freed individually, only all at once.  There can be multiple
   allocators, though.
   */

/* Returns a new memory allocator or NULL if out of memory. */
tre_mem_t tre_mem_new(void)
{
    tre_mem_t mem;

    mem = M_malloc_zero(sizeof(*mem));
    return mem;
}


/* Frees the memory allocator and all memory allocated with it. */
void tre_mem_destroy(tre_mem_t mem)
{
    tre_list_t *tmp;
    tre_list_t *l = mem->blocks;

    while (l != NULL) {
        M_free(l->data);
        tmp = l->next;
        M_free(l);
        l   = tmp;
    }

    M_free(mem);
}


/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block. */
void *tre_mem_alloc(tre_mem_t mem, size_t size)
{
    void *ptr;

    if (mem->n < size) {
        /* We need more memory than is available in the current block.
           Allocate a new block. */
        tre_list_t *l;
        size_t      block_size;

        if (size * 8 > TRE_MEM_BLOCK_SIZE) {
            block_size = size * 8;
        } else {
            block_size = TRE_MEM_BLOCK_SIZE;
        }

        l       = M_malloc(sizeof(*l));
        l->data = M_malloc(block_size);
        l->next = NULL;

        if (mem->current != NULL) {
            mem->current->next = l;
        }
        if (mem->blocks == NULL) {
            mem->blocks = l;
        }

        mem->current = l;
        mem->ptr = l->data;
        mem->n = block_size;
    }

    /* Make sure the next pointer will be aligned. */
    size     += ALIGN(mem->ptr + size, long);

    /* Allocate from current block. */
    ptr       = mem->ptr;
    mem->ptr += size;
    mem->n   -= size;

    return ptr;
}

void *tre_mem_calloc(tre_mem_t mem, size_t size)
{
    void *ptr;

    ptr = tre_mem_alloc(mem, size);
    M_mem_set(ptr, 0, size);
    return ptr;
}
