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
   regcomp.c - TRE POSIX compatible regex compilation functions.

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

#define MRE_DUP_MAX 255
/* Maximum length of a character class (text inside of [::]).
 * Anything longer will result in an error. */
#define MCHARCLASS_NAME_MAX 14

typedef struct {
	int           position;
	int           code_min;
	int           code_max;
	int          *tags;
	tre_assert_t  assertions;
} tre_pos_and_tags_t;


static M_uint32 tre_tolower(M_uint32 cp)
{
	M_uint32 lcp;
	if (M_utf8_tolower_cp(cp, &lcp) == M_UTF8_ERROR_SUCCESS)
		return lcp;
	return cp;
}

static M_uint32 tre_toupper(M_uint32 cp)
{
	M_uint32 lcp;
	if (M_utf8_toupper_cp(cp, &lcp) == M_UTF8_ERROR_SUCCESS)
		return lcp;
	return cp;
}

/***********************************************************************
  from tre-ast.c and tre-ast.h
 ***********************************************************************/

/* The different AST node types. */
typedef enum {
	LITERAL,
	CATENATION,
	ITERATION,
	UNION
} tre_ast_type_t;

/* Special subtypes of TRE_LITERAL. */
#define EMPTY     -1   /* Empty leaf (denotes empty string). */
#define ASSERTION -2   /* Assertion leaf. */
#define TAG       -3   /* Tag leaf. */

#define IS_SPECIAL(x)   ((x)->code_min < 0)
#define IS_TAG(x)       ((x)->code_min == TAG)


/* A generic AST node.  All AST nodes consist of this node on the top
   level with `obj' pointing to the actual content. */
typedef struct {
	tre_ast_type_t      type;   /* Type of the node. */
	void               *obj;             /* Pointer to actual node. */
	int                 nullable;
	int                 submatch_id;
	int                 num_submatches;
	int                 num_tags;
	tre_pos_and_tags_t *firstpos;
	tre_pos_and_tags_t *lastpos;
} tre_ast_node_t;


/* A "literal" node.  These are created for assertions, back references,
   tags, matching parameter settings, and all expressions that match one
   character. */
typedef struct {
	int          code_min;
	int          code_max;
	int          position;
} tre_literal_t;

/* A "catenation" node. These are created when two regexps are concatenated.
   If there are more than one subexpressions in sequence, the `left' part
   holds all but the last, and `right' part holds the last subexpression
   (catenation is left associative). */
typedef struct {
	tre_ast_node_t *left;
	tre_ast_node_t *right;
} tre_catenation_t;

/* An "iteration" node. These are created for the "*", "+", "?", and "{m,n}"
   operators. */
typedef struct {
	/* Subexpression to match. */
	tre_ast_node_t *arg;
	/* Minimum number of consecutive matches. */
	int min;
	/* Maximum number of consecutive matches. */
	int max;
	/* If 0, match as many characters as possible, if 1 match as few as
	   possible Note that this does not always mean the same thing as
	   matching as many/few repetitions as possible. */
	int minimal;
} tre_iteration_t;

/* An "union" node.  These are created for the "|" operator. */
typedef struct {
	tre_ast_node_t *left;
	tre_ast_node_t *right;
} tre_union_t;


static tre_ast_node_t *tre_ast_new_node(tre_mem_t mem, tre_ast_type_t type, void *obj)
{
	tre_ast_node_t *node = tre_mem_calloc(mem, sizeof *node);
	if (!node || !obj)
		return NULL;

	node->obj         = obj;
	node->type        = type;
	node->nullable    = -1;
	node->submatch_id = -1;

	return node;
}

static tre_ast_node_t *tre_ast_new_literal(tre_mem_t mem, int code_min, int code_max, int position)
{
	tre_ast_node_t *node;
	tre_literal_t  *lit;

	lit  = tre_mem_calloc(mem, sizeof *lit);
	node = tre_ast_new_node(mem, LITERAL, lit);
	if (!node)
		return NULL;

	lit->code_min = code_min;
	lit->code_max = code_max;
	lit->position = position;

	return node;
}

static tre_ast_node_t *tre_ast_new_iter(tre_mem_t mem, tre_ast_node_t *arg, int min, int max, int minimal)
{
	tre_ast_node_t  *node;
	tre_iteration_t *iter;

	iter = tre_mem_calloc(mem, sizeof *iter);
	node = tre_ast_new_node(mem, ITERATION, iter);
	if (!node)
		return NULL;

	iter->arg            = arg;
	iter->min            = min;
	iter->max            = max;
	iter->minimal        = minimal;
	node->num_submatches = arg->num_submatches;

	return node;
}

static tre_ast_node_t *tre_ast_new_union(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right)
{
	tre_ast_node_t *node;
	tre_union_t   *un;

	if (!left)
		return right;

	un   = tre_mem_calloc(mem, sizeof *un);
	node = tre_ast_new_node(mem, UNION, un);
	if (!node || !right)
		return NULL;

	un->left             = left;
	un->right            = right;
	node->num_submatches = left->num_submatches + right->num_submatches;

	return node;
}

static tre_ast_node_t *tre_ast_new_catenation(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right)
{
	tre_ast_node_t   *node;
	tre_catenation_t *cat;

	if (!left)
		return right;

	cat  = tre_mem_calloc(mem, sizeof *cat);
	node = tre_ast_new_node(mem, CATENATION, cat);
	if (!node)
		return NULL;

	cat->left            = left;
	cat->right           = right;
	node->num_submatches = left->num_submatches + right->num_submatches;

	return node;
}


/***********************************************************************
  from tre-stack.c and tre-stack.h
 ***********************************************************************/

typedef struct tre_stack_rec tre_stack_t;

/* Creates a new stack object.`size' is initial size in bytes, `max_size'
   is maximum size, and `increment' specifies how much more space will be
   allocated with realloc() if all space gets used up. Returns the stack
   object or NULL if out of memory. */
static tre_stack_t *tre_stack_new(size_t size, size_t max_size, size_t increment);

/* Frees the stack object. */
static void tre_stack_destroy(tre_stack_t *s);

/* Returns the current number of objects in the stack. */
static size_t tre_stack_num_objects(tre_stack_t *s);

/* Each tre_stack_push_*(tre_stack_t *s, <type> value) function pushes
   `value' on top of stack `s'.  Returns REG_ESPACE if out of memory.
   This tries to realloc() more space before failing if maximum size
   has not yet been reached.  Returns REG_OK if successful. */
#define declare_pushf(typetag, type) static reg_errcode_t tre_stack_push_ ## typetag(tre_stack_t *s, type value)

declare_pushf(voidptr, void *);
declare_pushf(int, int);

/* Each tre_stack_pop_*(tre_stack_t *s) function pops the topmost
   element off of stack `s' and returns it.  The stack must not be
   empty. */
#define declare_popf(typetag, type) static type tre_stack_pop_ ## typetag(tre_stack_t *s)

declare_popf(voidptr, void *);
declare_popf(int, int);

/* Just to save some typing. */
#define STACK_PUSH(s, typetag, value)     \
	do      \
{      \
	status = tre_stack_push_ ## typetag(s, value);      \
}      \
while (/*CONSTCOND*/0)

#define STACK_PUSHX(s, typetag, value)      \
{     \
	status = tre_stack_push_ ## typetag(s, value);      \
	if (status != REG_OK)      \
	break;    \
}

#define STACK_PUSHR(s, typetag, value)      \
{      \
	reg_errcode_t _status;     \
	_status = tre_stack_push_ ## typetag(s, value);     \
	if (_status != REG_OK)     \
	return _status;     \
}

union tre_stack_item {
	void *voidptr_value;
	int   int_value;
};

struct tre_stack_rec {
	size_t size;
	size_t max_size;
	size_t increment;
	size_t ptr;
	union tre_stack_item *stack;
};


static tre_stack_t *tre_stack_new(size_t size, size_t max_size, size_t increment)
{
	tre_stack_t *s;

	s            = M_malloc(sizeof(*s));
	s->stack     = M_malloc(sizeof(*s->stack) * size);
	s->size      = size;
	s->max_size  = max_size;
	s->increment = increment;
	s->ptr       = 0;
	return s;
}

static void tre_stack_destroy(tre_stack_t *s)
{
	M_free(s->stack);
	M_free(s);
}

static size_t tre_stack_num_objects(tre_stack_t *s)
{
	return s->ptr;
}

static reg_errcode_t tre_stack_push(tre_stack_t *s, union tre_stack_item value)
{
	union tre_stack_item *new_buffer;
	size_t                new_size;

	if (s->ptr < s->size) {
		s->stack[s->ptr] = value;
		s->ptr++;
		return REG_OK;
	}

	if (s->size >= s->max_size)
		return REG_ESPACE;

	new_size = s->size + s->increment;
	if (new_size > s->max_size)
		new_size = s->max_size;

	new_buffer = M_realloc(s->stack, sizeof(*new_buffer) * new_size);

	s->size  = new_size;
	s->stack = new_buffer;
	tre_stack_push(s, value);

	return REG_OK;
}

#define define_pushf(typetag, type)  \
	declare_pushf(typetag, type) {     \
		union tre_stack_item item;    \
		item.typetag ## _value = value;  \
		return tre_stack_push(s, item);  \
	}

	define_pushf(int, int)
define_pushf(voidptr, void *)

#define define_popf(typetag, type)   \
		declare_popf(typetag, type) {    \
			return s->stack[--s->ptr].typetag ## _value;    \
		}

define_popf(int, int)
define_popf(voidptr, void *)


/***********************************************************************
  from tre-parse.c and tre-parse.h
 ***********************************************************************/

/* Parse context. */
typedef struct {
	/* Memory allocator. The AST is allocated using this. */
	tre_mem_t mem;
	/* Stack used for keeping track of regexp syntax. */
	tre_stack_t *stack;
	/* The parsed node after a parse function returns. */
	tre_ast_node_t *n;
	/* Position in the regexp pattern after a parse function returns. */
	const char *s;
	/* The first character of the last subexpression parsed. */
	const char *start;
	/* Current submatch ID. */
	int submatch_id;
	/* Current position (number of literal). */
	int position;
	/* Compilation flags. */
	regex_flags_t cflags;
} tre_parse_ctx_t;

/* Some macros for expanding \w, \s, etc. */
static const struct {
	char        c;
	const char *expansion;
} tre_macros[] = {
	{'t', "\t"}, {'n', "\n"}, {'r', "\r"},
	{'f', "\f"}, {'a', "\a"}, {'e', "\033"},
	{'w', "[[:word:]]"}, {'W', "[^[:word:]]"}, {'s', "[[:space:]]"},
	{'S', "[^[:space:]]"}, {'d', "[[:digit:]]"}, {'D', "[^[:digit:]]"},
	{ 0, 0 }
};

/* Expands a macro delimited by `regex' and `regex_end' to `buf', which
   must have at least `len' items.  Sets buf[0] to zero if the there
   is no match in `tre_macros'. */
static const char *tre_expand_macro(const char *s)
{
	size_t i;

	for (i=0; tre_macros[i].c && tre_macros[i].c!=*s; i++)
		;

	return tre_macros[i].expansion;
}

static int tre_compare_lit(const void *a, const void *b, void *thunk)
{
	const tre_literal_t *const *la = a;
	const tre_literal_t *const *lb = b;

	(void)thunk;

	/* assumes the range of valid code_min is < INT_MAX */
	return la[0]->code_min - lb[0]->code_min;
}

struct literals {
	tre_mem_t       mem;
	tre_literal_t **a;
	size_t          len;
	size_t          cap;
};

static tre_literal_t *tre_new_lit(struct literals *p)
{
	tre_literal_t **a;

	if (p->len >= p->cap) {
		if (p->cap >= 1<<15) {
			return NULL;
		}

		p->cap *= 2;
		p->a    = M_realloc(p->a, p->cap * sizeof *p->a);
	}

	a  = p->a + p->len++;
	*a = tre_mem_calloc(p->mem, sizeof **a);

	return *a;
}

static M_bool add_icase_literals(struct literals *ls, int min, int max)
{
	tre_literal_t *lit;
	int            b;
	int            e;
	int            c;

	for (c=min; c<=max; ) {
		/* assumes islower(c) and isupper(c) are exclusive
		   and toupper(c)!=c if islower(c).
		   multiple opposite case characters are not supported */
		if (M_utf8_islower_cp((M_uint32)c)) {
			b = e = (int)tre_toupper((M_uint32)c);
			for (c++, e++; c<=max; c++, e++) {
				if ((int)tre_toupper((M_uint32)c) != e) {
					break;
				}
			}
		} else if (M_utf8_isupper_cp((M_uint32)c)) {
			b = e = (int)tre_tolower((M_uint32)c);
			for (c++, e++; c<=max; c++, e++) {
				if ((int)tre_tolower((M_uint32)c) != e) {
					break;
				}
			}
		} else {
			c++;
			continue;
		}

		lit = tre_new_lit(ls);
		if (lit == NULL)
			return M_FALSE; 

		lit->code_min = (int)b;
		lit->code_max = (int)e-1;
		lit->position = -1;
	}

	return M_TRUE;
}

/*
   bracket grammar:
   Bracket  =  '[' List ']'  |  '[^' List ']'
   List     =  Term  |  List Term
   Term     =  Char  |  Range  |  Chclass  |  Eqclass
   Range    =  Char '-' Char  |  Char '-' '-'
   Char     =  Coll  |  coll_single
   Meta     =  ']'  |  '-'
   Coll     =  '[.' coll_single '.]'  |  '[.' coll_multi '.]'  |  '[.' Meta '.]'
   Eqclass  =  '[=' coll_single '=]'  |  '[=' coll_multi '=]'
   Chclass  =  '[:' class ':]'

   coll_single is a single char collating element but it can be
   '-' only at the beginning or end of a List and
   ']' only at the beginning of a List and
   '^' anywhere except after the openning '['
*/

static reg_errcode_t parse_bracket_character_range(tre_parse_ctx_t *ctx, const char *range, struct literals *ls)
{
	const char    *s = range;
	const char    *next;
	tre_literal_t *lit;
	int            min;
	int            max;
	M_uint32       cp;

	if (*s == '\0')
		return REG_EBRACK;

	while (*s != '\0') {
		if (M_utf8_get_cp(s, &cp, &next) != M_UTF8_ERROR_SUCCESS) {
			return REG_EBRACK;
		}

		s   = next;
		min = max = (int)cp;

		if (*s == '-') {
			s++;
			if (M_utf8_get_cp(s, &cp, &next) != M_UTF8_ERROR_SUCCESS) {
				return REG_ERANGE;
			}

			s   = next;
			max = (int)cp;
			if (min > max) {
				return REG_ERANGE;
			}
		}

		lit = tre_new_lit(ls);
		if (lit == NULL) {
			return REG_ESPACE;
		}
		lit->code_min = min;
		lit->code_max = max;
		lit->position = -1;

		if (ctx->cflags & REG_ICASE) {
			if (!add_icase_literals(ls, min, max)) {
				return REG_ESPACE;
			}
		}
	}

	return REG_OK;
}

static const struct {
	const char *c_class;
	const char *expansion;
} c_class_macros[] = {
	{ "alpha", "a-zA-Z" },
	{ "alnum", "a-zA-Z0-9" },
	{ "word", "a-zA-Z0-9_" },
	{ "space", " \t\r\n\v\f" },
	{ "digit", "0-9" },
	/* \x00 is a control character but since we're dealing with strings,
 	 * we'll never encounter it because it's the end of string marker.
	 * We start at \x01 otherwise this string will end prematurely. */
	{ "cntrl", "\x01-\x1F\x7F" },
	{ "print", "\x20-\x7E" },
	{ "xdigit", "0-9a-fA-F" },
	{ "lower", "a-z" },
	{ "upper", "A-Z" },
	{ "blank", " \t" },
	{ "graph", "\x21-\x7E" },
	{ "punct", "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" },
	{ 0, 0 }
};

static reg_errcode_t parse_bracket_character_class(tre_parse_ctx_t *ctx, const char *c_class, struct literals *ls)
{
	size_t i;

	for (i=0; c_class_macros[i].c_class!=NULL; i++) {
		if (M_str_eq(c_class_macros[i].c_class, c_class)) {
			return parse_bracket_character_range(ctx, c_class_macros[i].expansion, ls);
		}
	}
	
	return REG_ECTYPE;
}

static reg_errcode_t parse_bracket_terms(tre_parse_ctx_t *ctx, const char *s, struct literals *ls)
{
	const char    *start = s;
	const char    *next;
	tre_literal_t *lit;
	int            min;
	int            max;
	int            len;
	M_uint32       cp;
	reg_errcode_t  ret;

	for (;;) {
		if (*s == ']' && s != start) {
			ctx->s = s+1;
			return REG_OK;
		}

		if (*s == '-' && s != start && s[1] != ']' &&
				/* extension: [a-z--@] is accepted as [a-z]|[--@] */
				(s[1] != '-' || s[2] == ']'))
		{
			return REG_ERANGE;
		}

		if (*s == '[' && (s[1] == '.' || s[1] == '='))
			/* collating symbols and equivalence classes are not supported */
		{
			return REG_ECOLLATE;
		}

		if (*s == '[' && s[1] == ':') {
			char tmp[MCHARCLASS_NAME_MAX+1];
			ret = REG_ECTYPE;

			s += 2;
			for (len=0; len<MCHARCLASS_NAME_MAX && s[len]; len++) {
				if (s[len] == ':') {
					M_mem_copy(tmp, s, (size_t)len);
					tmp[len] = 0;
					ret      = parse_bracket_character_class(ctx, tmp, ls);
					break;
				}
			}

			if (ret != REG_OK || s[len+1] != ']') {
				return REG_ECTYPE;
			}

			s  += len+2;
			continue;
		} else {
			if (M_utf8_get_cp(s, &cp, &next) != M_UTF8_ERROR_SUCCESS) {
				return REG_EBRACK;
			}

			min = max = (int)cp;
			s   = next;

			if (*s == '-' && s[1] != ']') {
				s++;
				if (M_utf8_get_cp(s, &cp, &next) != M_UTF8_ERROR_SUCCESS) {
					return REG_EBRACK;
				}
				max = (int)cp;
				/* XXX - Should use collation order instead of
				   encoding values in character ranges. */
				if (min > max) {
					return REG_ERANGE;
				}
				s = next;
			}
		}

		lit = tre_new_lit(ls);
		if (lit == NULL) {
			return REG_ESPACE;
		}
		lit->code_min = min;
		lit->code_max = max;
		lit->position = -1;

		/* Add opposite-case codepoints if REG_ICASE is present.
		   It seems that POSIX requires that bracket negation
		   should happen before case-folding, but most practical
		   implementations do it the other way around. Changing
		   the order would need efficient representation of
		   case-fold ranges and bracket range sets even with
		   simple patterns so this is ok for now. */
		if (ctx->cflags & REG_ICASE) {
			if (!add_icase_literals(ls, min, max)) {
				return REG_ESPACE;
			}
		}
	}

	return REG_OK;
}

static reg_errcode_t parse_bracket(tre_parse_ctx_t *ctx, const char *s)
{
	size_t           i;
	int              max;
	int              min;
	int              negmax = 0;
	int              negmin = 0;
	tre_ast_node_t  *node   = 0;
	tre_ast_node_t  *n;
	tre_literal_t   *lit;
	struct literals  ls;
	M_bool           negate = M_FALSE;
	reg_errcode_t    err;

	ls.mem     = ctx->mem;
	ls.len     = 0;
	ls.cap     = 32;
	ls.a       = M_malloc(ls.cap * sizeof *ls.a);

	if (*s == '^') {
		negate = M_TRUE;
		s++;
	}

	err = parse_bracket_terms(ctx, s, &ls);
	if (err != REG_OK)
		goto parse_bracket_done;

	if (negate) {
		if (ctx->cflags & REG_MULTILINE) {
			lit = tre_new_lit(&ls);
			if (lit == NULL) {
				err = REG_ESPACE;
				goto parse_bracket_done;
			}
			lit->code_min = '\n';
			lit->code_max = '\n';
			lit->position = -1;
		}

		/* Sort the array if we need to negate it. */
		M_sort_qsort(ls.a, ls.len, sizeof *ls.a, tre_compare_lit, NULL);

		/* extra lit for the last negated range */
		lit = tre_new_lit(&ls);
		if (lit == NULL) {
			err = REG_ESPACE;
			goto parse_bracket_done;
		}

		lit->code_min = TRE_CHAR_MAX+1;
		lit->code_max = TRE_CHAR_MAX+1;
		lit->position = -1;
	}

	/* Build a union of the items in the array, negated if necessary. */
	for (i=0; i<ls.len; i++) {
		lit = ls.a[i];
		min = lit->code_min;
		max = lit->code_max;

		if (negate) {
			if (min <= negmin) {
				/* Overlap. */
				negmin = M_MAX(max + 1, negmin);
				continue;
			}

			negmax        = min - 1;
			lit->code_min = negmin;
			lit->code_max = negmax;
			negmin        = max + 1;
		}

		lit->position    = ctx->position;
		n                = tre_ast_new_node(ctx->mem, LITERAL, lit);
		node             = tre_ast_new_union(ctx->mem, node, n);
		if (node == NULL) {
			err = REG_ESPACE;
			break;
		}
	}

parse_bracket_done:
	M_free(ls.a);
	ctx->position++;
	ctx->n = node;
	return err;
}

static const char *parse_dup_count(const char *s, int *n)
{
	if (!M_chr_isdigit(*s))
		return s;

	*n = 0;
	for (;;) {
		*n = 10 * *n + (*s - '0');
		s++;

		if (!M_chr_isdigit(*s) || *n > MRE_DUP_MAX) {
			break;
		}
	}

	return s;
}

static const char *parse_dup(const char *s, int *pmin, int *pmax)
{
	int min = 0;
	int max = -1;

	s = parse_dup_count(s, &min);
	if (*s == ',') {
		s = parse_dup_count(s+1, &max);
	} else {
		max = min;
	}

	if ((max < min && max >= 0)    ||
			max > MRE_DUP_MAX       ||
			min > MRE_DUP_MAX       ||
			min < 0                ||
			*s++ != '}')
	{
		return NULL;
	}

	*pmin = min;
	*pmax = max;
	return s;
}

static int hexval(int c)
{
	if (c-'0' < 10)
		return c-'0';

	c |= 32;
	if (c-'a' < 6)
		return c-'a'+10;

	return -1;
}

static reg_errcode_t marksub(tre_parse_ctx_t *ctx, tre_ast_node_t *node, int subid)
{
	if (node->submatch_id >= 0) {
		tre_ast_node_t *n = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
		if (n == NULL)
			return REG_ESPACE;

		n = tre_ast_new_catenation(ctx->mem, n, node);
		if (n == NULL)
			return REG_ESPACE;

		n->num_submatches = node->num_submatches;
		node = n;
	}

	node->submatch_id = subid;
	node->num_submatches++;
	ctx->n = node;
	return REG_OK;
}

/*
   ERE grammar:
   Regex  =  Branch  |  Regex '|' Branch
   Branch =  Atom  |  Branch Atom
   Atom   =  char  |  quoted_char  |  '.'  |  Bracket  |  Atom Dup  |  '(' Regex ')'  |  '^'  |  '$'
   Dup    =  '*'  |  '+'  |  '?'  |  '{' Count '}'  |  '{' Count ',}'  |  '{' Count ',' Count '}'

   (a*+?, ^*, $+, \X, {, (|a) are unspecified)
   */

static reg_errcode_t parse_atom(tre_parse_ctx_t *ctx, const char *s)
{
	int             len;
	const char     *p;
	const char     *next;
	tre_ast_node_t *node;
	M_uint32        cp;

	switch (*s) {
		case '[':
			return parse_bracket(ctx, s+1);
		case '\\':
			p = tre_expand_macro(s+1);
			if (p) {
				/* assume \X expansion is a single atom */
				reg_errcode_t err = parse_atom(ctx, p);
				ctx->s = s+2;
				return err;
			}
			/* extensions: \b, \B, \<, \>, \xHH \x{HHHH} */
			switch (*++s) {
				case 0:
					return REG_EESCAPE;
				case 'b':
					node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_WB, -1);
					break;
				case 'B':
					node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_WB_NEG, -1);
					break;
				case '<':
					node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_BOW, -1);
					break;
				case '>':
					node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_EOW, -1);
					break;
				case 'x':
					{
						int i;
						int v = 0;
						int c;
						s++;

						len = 2;
						if (*s == '{') {
							len = 8;
							s++;
						}
						for (i=0; i<len && v<0x110000; i++) {
							c = hexval(s[i]);
							if (c < 0) {
								break;
							}
							v = 16*v + c;
						}
						s += i;
						if (len == 8) {
							if (*s != '}') {
								return REG_EBRACE;
							}
							s++;
						}
						node = tre_ast_new_literal(ctx->mem, v, v, ctx->position++);
						s--;
					}
					break;
				case '{':
				case '+':
				case '?':
				case '|':
					/* fallthrough */
				default:
					goto parse_literal;
			}
			s++;
			break;
		case '.':
			if (ctx->cflags & REG_DOTALL) {
				node = tre_ast_new_literal(ctx->mem, 0, TRE_CHAR_MAX, ctx->position++);
			} else {
				tre_ast_node_t *tmp1, *tmp2;
				tmp1 = tre_ast_new_literal(ctx->mem, 0, '\n'-1, ctx->position++);
				tmp2 = tre_ast_new_literal(ctx->mem, '\n'+1, TRE_CHAR_MAX, ctx->position++);
				if (tmp1 && tmp2) {
					node = tre_ast_new_union(ctx->mem, tmp1, tmp2);
				} else {
					node = 0;
				}
			}
			s++;
			break;
		case '^':
			node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_BOL, -1);
			s++;
			break;
		case '$':
			node = tre_ast_new_literal(ctx->mem, ASSERTION, ASSERT_AT_EOL, -1);
			s++;
			break;
		case '*':
		case '{':
		case '+':
		case '?':
			/* reject repetitions after empty expression in ERE */
			return REG_BADRPT;
		case '|':
			/* Fall through. */
		case 0:
			node = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
			break;
		default:
parse_literal:
			if (M_utf8_get_cp(s, &cp, &next) != M_UTF8_ERROR_SUCCESS) {
				return REG_BADPAT;
			}

			if (ctx->cflags & REG_ICASE && (M_utf8_isupper_cp(cp) || M_utf8_islower_cp(cp))) {
				tre_ast_node_t *tmp1, *tmp2;
				/* multiple opposite case characters are not supported */
				tmp1 = tre_ast_new_literal(ctx->mem, (int)tre_toupper(cp), (int)tre_toupper(cp), ctx->position);
				tmp2 = tre_ast_new_literal(ctx->mem, (int)tre_tolower(cp), (int)tre_tolower(cp), ctx->position);
				if (tmp1 && tmp2) {
					node = tre_ast_new_union(ctx->mem, tmp1, tmp2);
				} else {
					node = 0;
				}
			} else {
				node = tre_ast_new_literal(ctx->mem, (int)cp, (int)cp, ctx->position);
			}
			ctx->position++;
			s = next;
			break;
	}

	if (!node)
		return REG_ESPACE;
	ctx->n = node;
	ctx->s = s;
	return REG_OK;
}

#define PUSHPTR(err, s, v) do { \
	if ((err = tre_stack_push_voidptr(s, v)) != REG_OK) \
	return err; \
} while(0)

#define PUSHINT(err, s, v) do { \
	if ((err = tre_stack_push_int(s, v)) != REG_OK) \
	return err; \
} while(0)

static reg_errcode_t tre_parse(tre_parse_ctx_t *ctx)
{
	tre_ast_node_t *nbranch = NULL;
	tre_ast_node_t *nunion  = NULL;
	const char     *start   = ctx->start;
	const char     *s       = ctx->start;
	int             subid   = 0;
	int             depth   = 0;
	reg_errcode_t   err;
	tre_stack_t    *stack   = ctx->stack;

	PUSHINT(err, stack, subid++);
	for (;;) {
		if (*s == '(') {
			if (*(s+1) == '?') {
				regex_flags_t new_cflags = ctx->cflags;
				M_bool        bit        = M_TRUE;

				/* Flags are only supported at the beginning of the expression. */
				if (s != start) {
					return REG_BADPAT;
				}

				s += 2;
				while (1) {
					if (*s == 'i') {
						if (bit) {
							new_cflags |= REG_ICASE;
						} else {
							new_cflags &= (regex_flags_t)(~REG_ICASE);
						}
						s++;
					} else if (*s == 'm') {
						if (bit) {
							new_cflags |= REG_MULTILINE;
						} else {
							new_cflags &= (regex_flags_t)(~REG_MULTILINE);
						}
						s++;
					} else if (*s == 's') {
						if (bit) {
							new_cflags |= REG_DOTALL;
						} else {
							new_cflags &= (regex_flags_t)(~REG_DOTALL);
						}
						s++;
					} else if (*s == 'U') {
						if (bit) {
							new_cflags |= REG_UNGREEDY;
						} else {
							new_cflags &= (regex_flags_t)(~REG_UNGREEDY);
						}
						s++;
					} else if (*s == '-') {
						s++;
						bit = M_FALSE;
					} else if (*s == ')') {
						s++;
						break;
					} else {
						return REG_BADPAT;
					}
				}
				ctx->cflags = new_cflags;
				continue;
			}

			PUSHPTR(err, stack, nunion);
			PUSHPTR(err, stack, nbranch);
			PUSHINT(err, stack, subid++);
			s++;
			depth++;
			nbranch    = 0;
			nunion     = 0;
			ctx->start = s;
			continue;
		}

		if (*s == ')' && depth) {
			ctx->n = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
			if (!ctx->n) {
				return REG_ESPACE;
			}
		} else {
			err = parse_atom(ctx, s);
			if (err != REG_OK) {
				return err;
			}
			s = ctx->s;
		}

parse_iter:
		for (;;) {
			int minimal = (ctx->cflags & REG_UNGREEDY) ? 1 : 0;
			int min;
			int max;

			if (*s != '*' && *s != '+' && *s != '?' && *s != '{')
				break;

			/* extension: multiple consecutive *+?{,} is unspecified,
			   but (a+)+ has to be supported so accepting a++ makes
			   sense, note however that the MRE_DUP_MAX limit can be
			   circumvented: (a{255}){255} uses a lot of memory.. */
			if (*s=='{') {
				s = parse_dup(s+1, &min, &max);
				if (s == NULL) {
					return REG_BADBR;
				}
			} else {
				min = 0;
				max = -1;

				if (*s == '+') {
					min = 1;
				}

				if (*s == '?') {
					max = 1;
				}

				s++;
			}

			if (*s == '?') {
				minimal = !(ctx->cflags & REG_UNGREEDY);
				s++;
			}

			if (max == 0) {
				ctx->n = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
			} else {
				ctx->n = tre_ast_new_iter(ctx->mem, ctx->n, min, max, minimal);
			}

			if (!ctx->n) {
				return REG_ESPACE;
			}
		}

		nbranch = tre_ast_new_catenation(ctx->mem, nbranch, ctx->n);
		if (*s == '|' || (*s == ')' && depth) || !*s) {
			/* extension: empty branch is unspecified (), (|a), (a|)
			   here they are not rejected but match on empty string */
			int c = *s;

			nunion  = tre_ast_new_union(ctx->mem, nunion, nbranch);
			nbranch = 0;

			if (c == '\\' && s[1] == '|') {
				s+=2;
				ctx->start = s;
			} else if (c == '|') {
				s++;
				ctx->start = s;
			} else {
				if (c == '\\') {
					if (!depth) {
						return REG_EPAREN;
					}
					s+=2;
				} else if (c == ')') {
					s++;
				}

				depth--;
				err = marksub(ctx, nunion, tre_stack_pop_int(stack));
				if (err != REG_OK) {
					return err;
				}

				if (!c && depth < 0) {
					ctx->submatch_id = subid;
					return REG_OK;
				}

				if (!c || depth < 0) {
					return REG_EPAREN;
				}

				nbranch = tre_stack_pop_voidptr(stack);
				nunion  = tre_stack_pop_voidptr(stack);
				goto parse_iter;
			}
		}
	}

	return REG_OK;
}


/***********************************************************************
  from tre-compile.c
 ***********************************************************************/


/*
   Algorithms to setup tags so that submatch addressing can be done.
   */


/* Inserts a catenation node to the root of the tree given in `node'.
   As the left child a new tag with number `tag_id' to `node' is added,
   and the right child is the old root. */
static reg_errcode_t tre_add_tag_left(tre_mem_t mem, tre_ast_node_t *node, int tag_id)
{
	tre_catenation_t *c;

	c = tre_mem_alloc(mem, sizeof(*c));
	if (c == NULL)
		return REG_ESPACE;

	c->left = tre_ast_new_literal(mem, TAG, tag_id, -1);
	if (c->left == NULL)
		return REG_ESPACE;

	c->right = tre_mem_alloc(mem, sizeof(tre_ast_node_t));
	if (c->right == NULL)
		return REG_ESPACE;

	c->right->obj            = node->obj;
	c->right->type           = node->type;
	c->right->nullable       = -1;
	c->right->submatch_id    = -1;
	c->right->firstpos       = NULL;
	c->right->lastpos        = NULL;
	c->right->num_tags       = 0;
	c->right->num_submatches = 0;
	node->obj                = c;
	node->type               = CATENATION;

	return REG_OK;
}

/* Inserts a catenation node to the root of the tree given in `node'.
   As the right child a new tag with number `tag_id' to `node' is added,
   and the left child is the old root. */
static reg_errcode_t tre_add_tag_right(tre_mem_t mem, tre_ast_node_t *node, int tag_id)
{
	tre_catenation_t *c;

	c = tre_mem_alloc(mem, sizeof(*c));
	if (c == NULL)
		return REG_ESPACE;

	c->right = tre_ast_new_literal(mem, TAG, tag_id, -1);
	if (c->right == NULL)
		return REG_ESPACE;

	c->left = tre_mem_alloc(mem, sizeof(tre_ast_node_t));
	if (c->left == NULL)
		return REG_ESPACE;

	c->left->obj            = node->obj;
	c->left->type           = node->type;
	c->left->nullable       = -1;
	c->left->submatch_id    = -1;
	c->left->firstpos       = NULL;
	c->left->lastpos        = NULL;
	c->left->num_tags       = 0;
	c->left->num_submatches = 0;
	node->obj               = c;
	node->type              = CATENATION;

	return REG_OK;
}

typedef enum {
	ADDTAGS_RECURSE,
	ADDTAGS_AFTER_ITERATION,
	ADDTAGS_AFTER_UNION_LEFT,
	ADDTAGS_AFTER_UNION_RIGHT,
	ADDTAGS_AFTER_CAT_LEFT,
	ADDTAGS_AFTER_CAT_RIGHT,
	ADDTAGS_SET_SUBMATCH_END
} tre_addtags_symbol_t;


typedef struct {
	int tag;
	int next_tag;
} tre_tag_states_t;


/* Go through `regset' and set submatch data for submatches that are
   using this tag. */
static void tre_purge_regset(int *regset, tre_tnfa_t *tnfa, int tag)
{
	int i;

	for (i=0; regset[i]>=0; i++) {
		int id    = regset[i] / 2;
		int start = !(regset[i] % 2);

		if (start) {
			tnfa->submatch_data[id].so_tag = tag;
		} else {
			tnfa->submatch_data[id].eo_tag = tag;
		}
	}
	regset[0] = -1;
}


/* Adds tags to appropriate locations in the parse tree in `tree', so that
   subexpressions marked for submatch addressing can be traced. */
static reg_errcode_t tre_add_tags(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *tree, tre_tnfa_t *tnfa)
{
	reg_errcode_t         status       = REG_OK;
	tre_addtags_symbol_t  symbol;
	tre_ast_node_t       *node         = tree; /* Tree node we are currently looking at. */
	size_t                bottom       = tre_stack_num_objects(stack);
	/* True for first pass (counting number of needed tags) */
	int                   first_pass   = (mem == NULL);
	int                  *regset;
	int                  *orig_regset;
	int                   num_tags     = 0; /* Total number of tags. */
	int                   num_minimals = 0; /* Number of special minimal tags. */
	int                   tag          = 0;    /* The tag that is to be added next. */
	int                   next_tag     = 1; /* Next tag to use after this one. */
	int                  *parents;    /* Stack of submatches the current submatch is contained in. */
	int                   minimal_tag  = -1; /* Tag that marks the beginning of a minimal match. */
	tre_tag_states_t     *saved_states;
	int                   id;
	int                   i;

	tre_tag_direction_t direction = TRE_TAG_MINIMIZE;
	if (!first_pass) {
		tnfa->end_tag         = 0;
		tnfa->minimal_tags[0] = -1;
	}

	regset = M_malloc(sizeof(*regset) * ((tnfa->num_submatches + 1) * 2));
	if (regset == NULL)
		return REG_ESPACE;

	regset[0]   = -1;
	orig_regset = regset;

	parents = M_malloc(sizeof(*parents) * (tnfa->num_submatches + 1));
	if (parents == NULL) {
		M_free(regset);
		return REG_ESPACE;
	}
	parents[0] = -1;

	saved_states = M_malloc(sizeof(*saved_states) * (tnfa->num_submatches + 1));
	if (saved_states == NULL) {
		M_free(regset);
		M_free(parents);
		return REG_ESPACE;
	} else {
		for (i = 0; i <= (int)tnfa->num_submatches; i++) {
			saved_states[i].tag = -1;
		}
	}

	STACK_PUSH(stack, voidptr, node);
	STACK_PUSH(stack, int, ADDTAGS_RECURSE);

	while (tre_stack_num_objects(stack) > bottom) {
		if (status != REG_OK)
			break;

		symbol = (tre_addtags_symbol_t)tre_stack_pop_int(stack);
		switch (symbol) {
			case ADDTAGS_SET_SUBMATCH_END:
				{
					id = tre_stack_pop_int(stack);

					/* Add end of this submatch to regset. */
					for (i = 0; regset[i] >= 0; i++)
						;

					regset[i]   = id * 2 + 1;
					regset[i+1] = -1;

					/* Pop this submatch from the parents stack. */
					for (i = 0; parents[i] >= 0; i++)
						;

					parents[i-1] = -1;
				}
				break;

			case ADDTAGS_RECURSE:
				node = tre_stack_pop_voidptr(stack);

				if (node->submatch_id >= 0) {
					id = node->submatch_id;

					/* Add start of this submatch to regset. */
					for (i = 0; regset[i] >= 0; i++);
					regset[i]   = id * 2;
					regset[i+1] = -1;

					if (!first_pass) {
						for (i = 0; parents[i] >= 0; i++)
							;

						tnfa->submatch_data[id].parents = NULL;
						if (i > 0) {
							int *p = M_malloc(sizeof(*p) * (size_t)(i + 1));

							if (p == NULL) {
								status = REG_ESPACE;
								break;
							}

							tnfa->submatch_data[id].parents = p;
							for (i = 0; parents[i] >= 0; i++) {
								p[i] = parents[i];
							}
							p[i] = -1;
						}
					}

					/* Add end of this submatch to regset after processing this
					   node. */
					STACK_PUSHX(stack, int, node->submatch_id);
					STACK_PUSHX(stack, int, ADDTAGS_SET_SUBMATCH_END);
				}

				switch (node->type) {
					case LITERAL:
						{
							tre_literal_t *lit = node->obj;

							if (!IS_SPECIAL(lit)) {
								if (regset[0] >= 0) {
									/* Regset is not empty, so add a tag before the literal. */
									if (!first_pass) {
										status                    = tre_add_tag_left(mem, node, tag);
										tnfa->tag_directions[tag] = direction;

										if (minimal_tag >= 0) {
											for (i = 0; tnfa->minimal_tags[i] >= 0; i++)
												;

											tnfa->minimal_tags[i]   = tag;
											tnfa->minimal_tags[i+1] = minimal_tag;
											tnfa->minimal_tags[i+2] = -1;
											minimal_tag             = -1;
											num_minimals++;
										}
										tre_purge_regset(regset, tnfa, tag);
									} else {
										node->num_tags = 1;
									}

									regset[0] = -1;
									tag       = next_tag;
									num_tags++;
									next_tag++;
								}
							}
						}
						break;

					case CATENATION:
						{
							tre_catenation_t *cat = node->obj;
							tre_ast_node_t *left  = cat->left;
							tre_ast_node_t *right = cat->right;
							int reserved_tag      = -1;


							/* After processing right child. */
							STACK_PUSHX(stack, voidptr, node);
							STACK_PUSHX(stack, int, ADDTAGS_AFTER_CAT_RIGHT);

							/* Process right child. */
							STACK_PUSHX(stack, voidptr, right);
							STACK_PUSHX(stack, int, ADDTAGS_RECURSE);

							/* After processing left child. */
							STACK_PUSHX(stack, int, next_tag + left->num_tags);
							if (left->num_tags > 0 && right->num_tags > 0) {
								/* Reserve the next tag to the right child. */
								reserved_tag = next_tag;
								next_tag++;
							}
							STACK_PUSHX(stack, int, reserved_tag);
							STACK_PUSHX(stack, int, ADDTAGS_AFTER_CAT_LEFT);

							/* Process left child. */
							STACK_PUSHX(stack, voidptr, left);
							STACK_PUSHX(stack, int, ADDTAGS_RECURSE);

						}
						break;

					case ITERATION:
						{
							tre_iteration_t *iter = node->obj;

							if (first_pass) {
								STACK_PUSHX(stack, int, regset[0] >= 0 || iter->minimal);
							} else {
								STACK_PUSHX(stack, int, tag);
								STACK_PUSHX(stack, int, iter->minimal);
							}
							STACK_PUSHX(stack, voidptr, node);
							STACK_PUSHX(stack, int, ADDTAGS_AFTER_ITERATION);

							STACK_PUSHX(stack, voidptr, iter->arg);
							STACK_PUSHX(stack, int, ADDTAGS_RECURSE);

							/* Regset is not empty, so add a tag here. */
							if (regset[0] >= 0 || iter->minimal) {
								if (!first_pass) {
									status = tre_add_tag_left(mem, node, tag);

									if (iter->minimal) {
										tnfa->tag_directions[tag] = TRE_TAG_MAXIMIZE;
									} else {
										tnfa->tag_directions[tag] = direction;
									}

									if (minimal_tag >= 0) {
										for (i = 0; tnfa->minimal_tags[i] >= 0; i++)
											;

										tnfa->minimal_tags[i]   = tag;
										tnfa->minimal_tags[i+1] = minimal_tag;
										tnfa->minimal_tags[i+2] = -1;
										minimal_tag             = -1;
										num_minimals++;
									}
									tre_purge_regset(regset, tnfa, tag);
								}

								regset[0] = -1;
								tag       = next_tag;
								num_tags++;
								next_tag++;
							}
							direction = TRE_TAG_MINIMIZE;
						}
						break;

					case UNION:
						{
							tre_union_t *uni      = node->obj;
							tre_ast_node_t *left  = uni->left;
							tre_ast_node_t *right = uni->right;
							int left_tag;
							int right_tag;

							if (regset[0] >= 0) {
								left_tag  = next_tag;
								right_tag = next_tag + 1;
							} else {
								left_tag  = tag;
								right_tag = next_tag;
							}

							/* After processing right child. */
							STACK_PUSHX(stack, int, right_tag);
							STACK_PUSHX(stack, int, left_tag);
							STACK_PUSHX(stack, voidptr, regset);
							STACK_PUSHX(stack, int, regset[0] >= 0);
							STACK_PUSHX(stack, voidptr, node);
							STACK_PUSHX(stack, voidptr, right);
							STACK_PUSHX(stack, voidptr, left);
							STACK_PUSHX(stack, int, ADDTAGS_AFTER_UNION_RIGHT);

							/* Process right child. */
							STACK_PUSHX(stack, voidptr, right);
							STACK_PUSHX(stack, int, ADDTAGS_RECURSE);

							/* After processing left child. */
							STACK_PUSHX(stack, int, ADDTAGS_AFTER_UNION_LEFT);

							/* Process left child. */
							STACK_PUSHX(stack, voidptr, left);
							STACK_PUSHX(stack, int, ADDTAGS_RECURSE);

							/* Regset is not empty, so add a tag here. */
							if (regset[0] >= 0) {
								if (!first_pass) {
									status                    = tre_add_tag_left(mem, node, tag);
									tnfa->tag_directions[tag] = direction;

									if (minimal_tag >= 0) {
										for (i = 0; tnfa->minimal_tags[i] >= 0; i++)
											;

										tnfa->minimal_tags[i]   = tag;
										tnfa->minimal_tags[i+1] = minimal_tag;
										tnfa->minimal_tags[i+2] = -1;
										minimal_tag             = -1;
										num_minimals++;
									}
									tre_purge_regset(regset, tnfa, tag);
								}

								regset[0] = -1;
								tag       = next_tag;
								num_tags++;
								next_tag++;
							}

							if (node->num_submatches > 0) {
								/* The next two tags are reserved for markers. */
								next_tag++;
								tag = next_tag;
								next_tag++;
							}
						}
						break;
				}

				if (node->submatch_id >= 0) {
					/* Push this submatch on the parents stack. */
					for (i = 0; parents[i] >= 0; i++)
						;

					parents[i] = node->submatch_id;
					parents[i + 1] = -1;
				}
				break; /* end case: ADDTAGS_RECURSE */

			case ADDTAGS_AFTER_ITERATION:
				{
					int minimal = 0;
					int enter_tag;

					node = tre_stack_pop_voidptr(stack);
					if (first_pass) {
						node->num_tags = ((tre_iteration_t *)node->obj)->arg->num_tags + tre_stack_pop_int(stack);
						minimal_tag    = -1;
					} else {
						minimal   = tre_stack_pop_int(stack);
						enter_tag = tre_stack_pop_int(stack);
						if (minimal) {
							minimal_tag = enter_tag;
						}
					}

					if (!first_pass) {
						if (minimal) {
							direction = TRE_TAG_MINIMIZE;
						} else {
							direction = TRE_TAG_MAXIMIZE;
						}
					}
				}
				break;

			case ADDTAGS_AFTER_CAT_LEFT:
				{
					int new_tag = tre_stack_pop_int(stack);
					next_tag    = tre_stack_pop_int(stack);
					if (new_tag >= 0) {
						tag = new_tag;
					}
				}
				break;

			case ADDTAGS_AFTER_CAT_RIGHT:
				node = tre_stack_pop_voidptr(stack);
				if (first_pass) {
					node->num_tags = ((tre_catenation_t *)node->obj)->left->num_tags + ((tre_catenation_t *)node->obj)->right->num_tags;
				}
				break;

			case ADDTAGS_AFTER_UNION_LEFT:
				/* Lift the bottom of the `regset' array so that when processing
				   the right operand the items currently in the array are
				   invisible. The original bottom was saved at ADDTAGS_UNION and
				   will be restored at ADDTAGS_AFTER_UNION_RIGHT below. */
				while (*regset >= 0) {
					regset++;
				}
				break;

			case ADDTAGS_AFTER_UNION_RIGHT:
				{
					int             added_tags;
					int             tag_left;
					int             tag_right;
					tre_ast_node_t *left = tre_stack_pop_voidptr(stack);
					tre_ast_node_t *right = tre_stack_pop_voidptr(stack);

					node       = tre_stack_pop_voidptr(stack);
					added_tags = tre_stack_pop_int(stack);

					if (first_pass) {
						node->num_tags = ((tre_union_t *)node->obj)->left->num_tags
							+ ((tre_union_t *)node->obj)->right->num_tags + added_tags
							+ ((node->num_submatches > 0) ? 2 : 0);
					}
					regset    = tre_stack_pop_voidptr(stack);
					tag_left  = tre_stack_pop_int(stack);
					tag_right = tre_stack_pop_int(stack);

					/* Add tags after both children, the left child gets a smaller
					   tag than the right child.  This guarantees that we prefer
					   the left child over the right child. */
					if (node->num_submatches > 0) {
						if (!first_pass) {
							status                         = tre_add_tag_right(mem, left, tag_left);
							tnfa->tag_directions[tag_left] = TRE_TAG_MAXIMIZE;

							if (status == REG_OK) {
								status = tre_add_tag_right(mem, right, tag_right);
							}
							tnfa->tag_directions[tag_right] = TRE_TAG_MAXIMIZE;
						}
						num_tags += 2;
					}
					direction = TRE_TAG_MAXIMIZE;
				}
				break;

			default:
				break;

		} /* end switch(symbol) */
	} /* end while(tre_stack_num_objects(stack) > bottom) */

	if (!first_pass)
		tre_purge_regset(regset, tnfa, tag);

	if (!first_pass && minimal_tag >= 0) {
		for (i = 0; tnfa->minimal_tags[i] >= 0; i++)
			;

		tnfa->minimal_tags[i]   = tag;
		tnfa->minimal_tags[i+1] = minimal_tag;
		tnfa->minimal_tags[i+2] = -1;
		num_minimals++;
	}

	tnfa->end_tag      = num_tags;
	tnfa->num_tags     = num_tags;
	tnfa->num_minimals = num_minimals;
	M_free(orig_regset);
	M_free(parents);
	M_free(saved_states);
	return status;
}



/*
   AST to TNFA compilation routines.
   */

typedef enum {
	COPY_RECURSE,
	COPY_SET_RESULT_PTR
} tre_copyast_symbol_t;

/* Flags for tre_copy_ast(). */
#define COPY_REMOVE_TAGS        1
#define COPY_MAXIMIZE_FIRST_TAG 2

static reg_errcode_t tre_copy_ast(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *ast,
		int flags, int *pos_add, tre_tag_direction_t *tag_directions,
		tre_ast_node_t **copy, int *max_pos)
{
	reg_errcode_t          status     = REG_OK;
	size_t                 bottom     = tre_stack_num_objects(stack);
	int                    num_copied = 0;
	int                    first_tag  = 1;
	tre_ast_node_t       **result     = copy;
	tre_copyast_symbol_t   symbol;

	STACK_PUSH(stack, voidptr, ast);
	STACK_PUSH(stack, int, COPY_RECURSE);

	while (status == REG_OK && tre_stack_num_objects(stack) > bottom) {
		tre_ast_node_t *node;

		symbol = (tre_copyast_symbol_t)tre_stack_pop_int(stack);
		switch (symbol) {
			case COPY_SET_RESULT_PTR:
				result = tre_stack_pop_voidptr(stack);
				break;

			case COPY_RECURSE:
				node = tre_stack_pop_voidptr(stack);
				switch (node->type) {
					case LITERAL:
						{
							tre_literal_t *lit = node->obj;
							int            pos = lit->position;
							int            min = lit->code_min;
							int            max = lit->code_max;

							if (!IS_SPECIAL(lit)) {
								/* [ab] has only one position but two
								   nodes, so we are creating holes in the state space
								   here.  Not fatal, just wastes memory. */
								pos += *pos_add;
								num_copied++;
							} else if (IS_TAG(lit) && (flags & COPY_REMOVE_TAGS)) {
								/* Change this tag to empty. */
								min = EMPTY;
								max = pos = -1;
							} else if (IS_TAG(lit) && (flags & COPY_MAXIMIZE_FIRST_TAG) && first_tag) {
								/* This will never be NULL due to it always being set with this
								 * flag. But we need to silence a warning. */
								if (tag_directions != NULL) {
									/* Maximize the first tag. */
									tag_directions[max] = TRE_TAG_LEFT_MAXIMIZE;
								}
								first_tag           = 0;
							}

							*result = tre_ast_new_literal(mem, min, max, pos);
							if (*result == NULL) {
								status = REG_ESPACE;
							}

							if (pos > *max_pos) {
								*max_pos = pos;
							}
						}
						break;

					case UNION:
						{
							tre_union_t *uni = node->obj;
							tre_union_t *tmp;

							*result = tre_ast_new_union(mem, uni->left, uni->right);
							if (*result == NULL) {
								status = REG_ESPACE;
								break;
							}

							tmp    = (*result)->obj;
							result = &tmp->left;
							STACK_PUSHX(stack, voidptr, uni->right);
							STACK_PUSHX(stack, int, COPY_RECURSE);
							STACK_PUSHX(stack, voidptr, &tmp->right);
							STACK_PUSHX(stack, int, COPY_SET_RESULT_PTR);
							STACK_PUSHX(stack, voidptr, uni->left);
							STACK_PUSHX(stack, int, COPY_RECURSE);
						}
						break;

					case CATENATION:
						{
							tre_catenation_t *cat = node->obj;
							tre_catenation_t *tmp;

							*result = tre_ast_new_catenation(mem, cat->left, cat->right);
							if (*result == NULL) {
								status = REG_ESPACE;
								break;
							}

							tmp        = (*result)->obj;
							tmp->left  = NULL;
							tmp->right = NULL;
							result     = &tmp->left;

							STACK_PUSHX(stack, voidptr, cat->right);
							STACK_PUSHX(stack, int, COPY_RECURSE);
							STACK_PUSHX(stack, voidptr, &tmp->right);
							STACK_PUSHX(stack, int, COPY_SET_RESULT_PTR);
							STACK_PUSHX(stack, voidptr, cat->left);
							STACK_PUSHX(stack, int, COPY_RECURSE);
						}
						break;

					case ITERATION:
						{
							tre_iteration_t *iter = node->obj;

							STACK_PUSHX(stack, voidptr, iter->arg);
							STACK_PUSHX(stack, int, COPY_RECURSE);

							*result = tre_ast_new_iter(mem, iter->arg, iter->min, iter->max, iter->minimal);
							if (*result == NULL) {
								status = REG_ESPACE;
								break;
							}

							iter   = (*result)->obj;
							result = &iter->arg;
						}
						break;

					default:
						break;
				}
				break; /* COPY_RECURSE */
		}
	}

	*pos_add += num_copied;
	return status;
}

typedef enum {
	EXPAND_RECURSE,
	EXPAND_AFTER_ITER
} tre_expand_ast_symbol_t;

/* Expands each iteration node that has a finite nonzero minimum or maximum
   iteration count to a catenated sequence of copies of the node. */
static reg_errcode_t tre_expand_ast(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *ast,
		int *position, tre_tag_direction_t *tag_directions)
{
	reg_errcode_t status        = REG_OK;
	size_t        bottom        = tre_stack_num_objects(stack);
	int           pos_add       = 0;
	int           pos_add_total = 0;
	int           max_pos       = 0;
	int           iter_depth    = 0;

	STACK_PUSHR(stack, voidptr, ast);
	STACK_PUSHR(stack, int, EXPAND_RECURSE);

	while (status == REG_OK && tre_stack_num_objects(stack) > bottom) {
		tre_ast_node_t          *node;
		tre_expand_ast_symbol_t  symbol;

		symbol = (tre_expand_ast_symbol_t)tre_stack_pop_int(stack);
		node   = tre_stack_pop_voidptr(stack);
		switch (symbol) {
			case EXPAND_RECURSE:
				switch (node->type) {
					case LITERAL:
						{
							tre_literal_t *lit= node->obj;
							if (!IS_SPECIAL(lit)) {
								lit->position += pos_add;
								if (lit->position > max_pos) {
									max_pos = lit->position;
								}
							}
						}
						break;

					case UNION:
						{
							tre_union_t *uni = node->obj;
							STACK_PUSHX(stack, voidptr, uni->right);
							STACK_PUSHX(stack, int, EXPAND_RECURSE);
							STACK_PUSHX(stack, voidptr, uni->left);
							STACK_PUSHX(stack, int, EXPAND_RECURSE);
						}
						break;

					case CATENATION:
						{
							tre_catenation_t *cat = node->obj;
							STACK_PUSHX(stack, voidptr, cat->right);
							STACK_PUSHX(stack, int, EXPAND_RECURSE);
							STACK_PUSHX(stack, voidptr, cat->left);
							STACK_PUSHX(stack, int, EXPAND_RECURSE);
						}
						break;

					case ITERATION:
						{
							tre_iteration_t *iter = node->obj;
							STACK_PUSHX(stack, int, pos_add);
							STACK_PUSHX(stack, voidptr, node);
							STACK_PUSHX(stack, int, EXPAND_AFTER_ITER);
							STACK_PUSHX(stack, voidptr, iter->arg);
							STACK_PUSHX(stack, int, EXPAND_RECURSE);
							/* If we are going to expand this node at EXPAND_AFTER_ITER
							   then don't increase the `pos' fields of the nodes now, it
							   will get done when expanding. */
							if (iter->min > 1 || iter->max > 1) {
								pos_add = 0;
							}
							iter_depth++;
						}
						break;

					default:
						break;
				}
				break;

			case EXPAND_AFTER_ITER:
				{
					tre_iteration_t *iter = node->obj;
					int              pos_add_last;

					pos_add      = tre_stack_pop_int(stack);
					pos_add_last = pos_add;

					if (iter->min > 1 || iter->max > 1) {
						tre_ast_node_t *seq1         = NULL;
						tre_ast_node_t *seq2         = NULL;
						int             pos_add_save = pos_add;
						int             j;

						/* Create a catenated sequence of copies of the node. */
						for (j = 0; j < iter->min; j++) {
							tre_ast_node_t *copy = NULL;

							/* Remove tags from all but the last copy. */
							int flags = ((j + 1 < iter->min)
									? COPY_REMOVE_TAGS
									: COPY_MAXIMIZE_FIRST_TAG);

							pos_add_save = pos_add;
							status       = tre_copy_ast(mem, stack, iter->arg, flags, &pos_add, tag_directions, &copy, &max_pos);
							if (status != REG_OK) {
								return status;
							}

							if (seq1 != NULL) {
								seq1 = tre_ast_new_catenation(mem, seq1, copy);
							} else {
								seq1 = copy;
							}

							if (seq1 == NULL) {
								return REG_ESPACE;
							}
						}

						if (iter->max == -1) {
							/* No upper limit. */
							pos_add_save = pos_add;
							status       = tre_copy_ast(mem, stack, iter->arg, 0, &pos_add, NULL, &seq2, &max_pos);
							if (status != REG_OK) {
								return status;
							}

							seq2 = tre_ast_new_iter(mem, seq2, 0, -1, 0);
							if (seq2 == NULL) {
								return REG_ESPACE;
							}
						} else {
							for (j = iter->min; j < iter->max; j++) {
								tre_ast_node_t *tmp;
								tre_ast_node_t *copy = NULL;

								pos_add_save = pos_add;
								status       = tre_copy_ast(mem, stack, iter->arg, 0, &pos_add, NULL, &copy, &max_pos);
								if (status != REG_OK) {
									return status;
								}

								if (seq2 != NULL) {
									seq2 = tre_ast_new_catenation(mem, copy, seq2);
								} else {
									seq2 = copy;
								}

								if (seq2 == NULL) {
									return REG_ESPACE;
								}

								tmp = tre_ast_new_literal(mem, EMPTY, -1, -1);
								if (tmp == NULL) {
									return REG_ESPACE;
								}

								seq2 = tre_ast_new_union(mem, tmp, seq2);
								if (seq2 == NULL) {
									return REG_ESPACE;
								}
							}
						}

						pos_add = pos_add_save;
						if (seq1 == NULL) {
							seq1 = seq2;
						} else if (seq2 != NULL) {
							seq1 = tre_ast_new_catenation(mem, seq1, seq2);
						}

						if (seq1 == NULL) {
							return REG_ESPACE;
						}

						node->obj  = seq1->obj;
						node->type = seq1->type;
					}

					iter_depth--;
					pos_add_total += pos_add - pos_add_last;
					if (iter_depth == 0) {
						pos_add = pos_add_total;
					}
				}
				break;

			default:
				break;
		}
	}

	*position += pos_add_total;

	/* `max_pos' should never be larger than `*position' if the above
	   code works, but just an extra safeguard let's make sure
	   `*position' is set large enough so enough memory will be
	   allocated for the transition table. */
	if (max_pos > *position)
		*position = max_pos;

	return status;
}

static tre_pos_and_tags_t *tre_set_empty(tre_mem_t mem)
{
	tre_pos_and_tags_t *new_set;

	new_set = tre_mem_calloc(mem, sizeof(*new_set));
	if (new_set == NULL)
		return NULL;

	new_set[0].position = -1;
	new_set[0].code_min = -1;
	new_set[0].code_max = -1;

	return new_set;
}

static tre_pos_and_tags_t *tre_set_one(tre_mem_t mem, int position, int code_min, int code_max)
{
	tre_pos_and_tags_t *new_set;

	new_set = tre_mem_calloc(mem, sizeof(*new_set) * 2);
	if (new_set == NULL)
		return NULL;

	new_set[0].position    = position;
	new_set[0].code_min    = code_min;
	new_set[0].code_max    = code_max;
	new_set[1].position    = -1;
	new_set[1].code_min    = -1;
	new_set[1].code_max    = -1;

	return new_set;
}

static tre_pos_and_tags_t *tre_set_union(tre_mem_t mem, tre_pos_and_tags_t *set1, tre_pos_and_tags_t *set2,
		int *tags, tre_assert_t assertions)
{
	size_t              s1;
	size_t              s2;
	size_t              i;
	size_t              j;
	tre_pos_and_tags_t *new_set;
	int                *new_tags;
	size_t              num_tags;

	for (num_tags = 0; tags != NULL && tags[num_tags] >= 0; num_tags++)
		;

	for (s1 = 0; set1[s1].position >= 0; s1++)
		;

	for (s2 = 0; set2[s2].position >= 0; s2++)
		;

	new_set = tre_mem_calloc(mem, sizeof(*new_set) * (s1 + s2 + 1));
	if (!new_set )
		return NULL;

	for (s1 = 0; set1[s1].position >= 0; s1++) {
		new_set[s1].position    = set1[s1].position;
		new_set[s1].code_min    = set1[s1].code_min;
		new_set[s1].code_max    = set1[s1].code_max;
		new_set[s1].assertions  = set1[s1].assertions | assertions;
		if (set1[s1].tags == NULL && tags == NULL) {
			new_set[s1].tags = NULL;
		} else {
			for (i = 0; set1[s1].tags != NULL && set1[s1].tags[i] >= 0; i++)
				;

			new_tags = tre_mem_alloc(mem, (sizeof(*new_tags) * (i + num_tags + 1)));
			if (new_tags == NULL)
				return NULL;

			for (j = 0; j < i; j++) {
				new_tags[j] = set1[s1].tags[j];
			}

			for (i = 0; i < num_tags; i++) {
				new_tags[j + i] = tags[i];
			}

			new_tags[j + i]  = -1;
			new_set[s1].tags = new_tags;
		}
	}

	for (s2 = 0; set2[s2].position >= 0; s2++) {
		new_set[s1 + s2].position = set2[s2].position;
		new_set[s1 + s2].code_min = set2[s2].code_min;
		new_set[s1 + s2].code_max = set2[s2].code_max;

		/* why not | assertions here as well? */
		new_set[s1 + s2].assertions  = set2[s2].assertions;

		if (set2[s2].tags == NULL) {
			new_set[s1 + s2].tags = NULL;
		} else {
			for (i = 0; set2[s2].tags[i] >= 0; i++)
				;

			new_tags = tre_mem_alloc(mem, sizeof(*new_tags) * (i + 1));
			if (new_tags == NULL) {
				return NULL;
			}

			for (j = 0; j < i; j++) {
				new_tags[j] = set2[s2].tags[j];
			}

			new_tags[j]           = -1;
			new_set[s1 + s2].tags = new_tags;
		}
	}

	new_set[s1 + s2].position = -1;
	return new_set;
}

/* Finds the empty path through `node' which is the one that should be
   taken according to POSIX.2 rules, and adds the tags on that path to
   `tags'.   `tags' may be NULL.  If `num_tags_seen' is not NULL, it is
   set to the number of tags seen on the path. */
static reg_errcode_t tre_match_empty(tre_stack_t *stack, tre_ast_node_t *node, int *tags,
		tre_assert_t *assertions, size_t *num_tags_seen)
{
	tre_literal_t    *lit;
	tre_union_t      *uni;
	tre_catenation_t *cat;
	tre_iteration_t  *iter;
	int               i;
	size_t            bottom = tre_stack_num_objects(stack);
	reg_errcode_t     status = REG_OK;

	if (num_tags_seen)
		*num_tags_seen = 0;

	status = tre_stack_push_voidptr(stack, node);

	/* Walk through the tree recursively. */
	while (status == REG_OK && tre_stack_num_objects(stack) > bottom) {
		node = tre_stack_pop_voidptr(stack);

		switch (node->type) {
			case LITERAL:
				lit = (tre_literal_t *)node->obj;
				switch (lit->code_min) {
					case TAG:
						if (lit->code_max >= 0) {
							if (tags != NULL) {
								/* Add the tag to `tags'. */
								for (i = 0; tags[i] >= 0; i++) {
									if (tags[i] == lit->code_max) {
										break;
									}
								}

								if (tags[i] < 0) {
									tags[i] = lit->code_max;
									tags[i + 1] = -1;
								}
							}
							if (num_tags_seen) {
								(*num_tags_seen)++;
							}
						}
						break;

					case ASSERTION:
						if (assertions != NULL) {
							/* The litteral is being abused and is having assertion
 							 * parameters stuffed into it. */
							*assertions |= (tre_assert_t)lit->code_max;
						}
						break;

					case EMPTY:
						break;

					default:
						break;
				}
				break;

			case UNION:
				/* Subexpressions starting earlier take priority over ones
				   starting later, so we prefer the left subexpression over the
				   right subexpression. */
				uni = (tre_union_t *)node->obj;
				if (uni->left->nullable) {
					STACK_PUSHX(stack, voidptr, uni->left)
				} else if (uni->right->nullable) {
					STACK_PUSHX(stack, voidptr, uni->right)
				}
				break;

			case CATENATION:
				/* The path must go through both children. */
				cat = (tre_catenation_t *)node->obj;
				STACK_PUSHX(stack, voidptr, cat->left);
				STACK_PUSHX(stack, voidptr, cat->right);
				break;

			case ITERATION:
				/* A match with an empty string is preferred over no match at
				   all, so we go through the argument if possible. */
				iter = (tre_iteration_t *)node->obj;
				if (iter->arg->nullable) {
					STACK_PUSHX(stack, voidptr, iter->arg);
				}
				break;

			default:
				break;
		}
	}

	return status;
}


typedef enum {
	NFL_RECURSE,
	NFL_POST_UNION,
	NFL_POST_CATENATION,
	NFL_POST_ITERATION
} tre_nfl_stack_symbol_t;


/* Computes and fills in the fields `nullable', `firstpos', and `lastpos' for
   the nodes of the AST `tree'. */
static reg_errcode_t tre_compute_nfl(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *tree)
{
	size_t bottom = tre_stack_num_objects(stack);

	STACK_PUSHR(stack, voidptr, tree);
	STACK_PUSHR(stack, int, NFL_RECURSE);

	while (tre_stack_num_objects(stack) > bottom) {
		tre_nfl_stack_symbol_t  symbol;
		tre_ast_node_t         *node;

		symbol = (tre_nfl_stack_symbol_t)tre_stack_pop_int(stack);
		node   = tre_stack_pop_voidptr(stack);
		switch (symbol) {
			case NFL_RECURSE:
				switch (node->type) {
					case LITERAL:
						{
							tre_literal_t *lit = (tre_literal_t *)node->obj;
							if (lit->code_min < 0) {
								/* Tags, empty strings, params, and zero width assertions:
								   nullable = true, firstpos = {}, and lastpos = {}. */
								node->nullable = 1;
								node->firstpos = tre_set_empty(mem);

								if (!node->firstpos) {
									return REG_ESPACE;
								}

								node->lastpos = tre_set_empty(mem);
								if (!node->lastpos) {
									return REG_ESPACE;
								}
							} else {
								/* Literal at position i: nullable = false, firstpos = {i},
								   lastpos = {i}. */
								node->nullable = 0;
								node->firstpos =
									tre_set_one(mem, lit->position, (int)lit->code_min, (int)lit->code_max);

								if (!node->firstpos) {
									return REG_ESPACE;
								}

								node->lastpos = tre_set_one(mem, lit->position, (int)lit->code_min, (int)lit->code_max);
								if (!node->lastpos) {
									return REG_ESPACE;
								}
							}
						}
						break;

					case UNION:
						/* Compute the attributes for the two subtrees, and after that
						   for this node. */
						STACK_PUSHR(stack, voidptr, node);
						STACK_PUSHR(stack, int, NFL_POST_UNION);
						STACK_PUSHR(stack, voidptr, ((tre_union_t *)node->obj)->right);
						STACK_PUSHR(stack, int, NFL_RECURSE);
						STACK_PUSHR(stack, voidptr, ((tre_union_t *)node->obj)->left);
						STACK_PUSHR(stack, int, NFL_RECURSE);
						break;

					case CATENATION:
						/* Compute the attributes for the two subtrees, and after that
						   for this node. */
						STACK_PUSHR(stack, voidptr, node);
						STACK_PUSHR(stack, int, NFL_POST_CATENATION);
						STACK_PUSHR(stack, voidptr, ((tre_catenation_t *)node->obj)->right);
						STACK_PUSHR(stack, int, NFL_RECURSE);
						STACK_PUSHR(stack, voidptr, ((tre_catenation_t *)node->obj)->left);
						STACK_PUSHR(stack, int, NFL_RECURSE);
						break;

					case ITERATION:
						/* Compute the attributes for the subtree, and after that for
						   this node. */
						STACK_PUSHR(stack, voidptr, node);
						STACK_PUSHR(stack, int, NFL_POST_ITERATION);
						STACK_PUSHR(stack, voidptr, ((tre_iteration_t *)node->obj)->arg);
						STACK_PUSHR(stack, int, NFL_RECURSE);
						break;
				}
				break; /* end case: NFL_RECURSE */

			case NFL_POST_UNION:
				{
					tre_union_t *uni = (tre_union_t *)node->obj;
					node->nullable   = uni->left->nullable || uni->right->nullable;
					node->firstpos   = tre_set_union(mem, uni->left->firstpos, uni->right->firstpos, NULL, 0);

					if (!node->firstpos) {
						return REG_ESPACE;
					}

					node->lastpos = tre_set_union(mem, uni->left->lastpos, uni->right->lastpos, NULL, 0);
					if (!node->lastpos) {
						return REG_ESPACE;
					}
				}
				break;

			case NFL_POST_ITERATION:
				{
					tre_iteration_t *iter = (tre_iteration_t *)node->obj;

					if (iter->min == 0 || iter->arg->nullable) {
						node->nullable = 1;
					} else {
						node->nullable = 0;
					}
					node->firstpos = iter->arg->firstpos;
					node->lastpos = iter->arg->lastpos;
				}
				break;

			case NFL_POST_CATENATION:
				{
					size_t            num_tags;
					int              *tags;
					tre_assert_t      assertions;
					reg_errcode_t     status;
					tre_catenation_t *cat = node->obj;

					node->nullable = cat->left->nullable && cat->right->nullable;

					/* Compute firstpos. */
					if (cat->left->nullable) {
						/* The left side matches the empty string.  Make a first pass
						   with tre_match_empty() to get the number of tags and
						   parameters. */
						status = tre_match_empty(stack, cat->left, NULL, NULL, &num_tags);
						if (status != REG_OK) {
							return status;
						}

						/* Allocate arrays for the tags and parameters. */
						tags = M_malloc(sizeof(*tags) * (num_tags + 1));
						if (!tags) {
							return REG_ESPACE;
						}
						tags[0]    = -1;
						assertions = 0;

						/* Second pass with tre_mach_empty() to get the list of
						   tags and parameters. */
						status = tre_match_empty(stack, cat->left, tags, &assertions, NULL);
						if (status != REG_OK) {
							M_free(tags);
							return status;
						}

						node->firstpos =
							tre_set_union(mem, cat->right->firstpos, cat->left->firstpos,
									tags, assertions);

						M_free(tags);
						if (!node->firstpos) {
							return REG_ESPACE;
						}
					} else {
						node->firstpos = cat->left->firstpos;
					}

					/* Compute lastpos. */
					if (cat->right->nullable) {
						/* The right side matches the empty string.  Make a first pass
						   with tre_match_empty() to get the number of tags and
						   parameters. */
						status = tre_match_empty(stack, cat->right, NULL, NULL, &num_tags);
						if (status != REG_OK) {
							return status;
						}

						/* Allocate arrays for the tags and parameters. */
						tags = M_malloc(sizeof(int) * (num_tags + 1));
						if (!tags) {
							return REG_ESPACE;
						}
						tags[0] = -1;
						assertions = 0;

						/* Second pass with tre_mach_empty() to get the list of
						   tags and parameters. */
						status = tre_match_empty(stack, cat->right, tags, &assertions, NULL);
						if (status != REG_OK) {
							M_free(tags);
							return status;
						}

						node->lastpos =
							tre_set_union(mem, cat->left->lastpos, cat->right->lastpos,
									tags, assertions);

						M_free(tags);
						if (!node->lastpos) {
							return REG_ESPACE;
						}
					} else {
						node->lastpos = cat->right->lastpos;
					}
				}
				break;

			default:
				break;
		}
	}

	return REG_OK;
}


/* Adds a transition from each position in `p1' to each position in `p2'. */
static reg_errcode_t tre_make_trans(tre_pos_and_tags_t *p1, tre_pos_and_tags_t *p2,
		tre_tnfa_transition_t *transitions,
		int *counts, int *offs)
{
	tre_pos_and_tags_t    *orig_p2 = p2;
	tre_tnfa_transition_t *trans;
	size_t                 i;
	size_t                 j;
	size_t                 k;
	size_t                 l;
	int                    dup;
	int                    prev_p2_pos;

	if (transitions != NULL) {
		while (p1->position >= 0) {
			p2          = orig_p2;
			prev_p2_pos = -1;

			while (p2->position >= 0) {
				/* Optimization: if this position was already handled, skip it. */
				if (p2->position == prev_p2_pos) {
					p2++;
					continue;
				}

				prev_p2_pos = p2->position;
				/* Set `trans' to point to the next unused transition from
				   position `p1->position'. */
				trans = transitions + offs[p1->position];
				while (trans->state != NULL) {
					trans++;
				}

				if (trans->state == NULL) {
					(trans + 1)->state = NULL;
				}

				/* Use the character ranges, assertions, etc. from `p1' for
				   the transition from `p1' to `p2'. */
				trans->code_min   = p1->code_min;
				trans->code_max   = p1->code_max;
				trans->state      = transitions + offs[p2->position];
				trans->state_id   = p2->position;
				trans->assertions = p1->assertions | p2->assertions;

				/* Find out how many tags this transition has. */
				i = 0;
				if (p1->tags != NULL) {
					while(p1->tags[i] >= 0) {
						i++;
					}
				}

				j = 0;
				if (p2->tags != NULL) {
					while(p2->tags[j] >= 0) {
						j++;
					}
				}

				/* If we are overwriting a transition, free the old tag array. */
				if (trans->tags != NULL) {
					M_free(trans->tags);
				}
				trans->tags = NULL;

				/* If there were any tags, allocate an array and fill it. */
				if (i + j > 0) {
					trans->tags = M_malloc(sizeof(*trans->tags) * (i + j + 1));
					if (!trans->tags) {
						return REG_ESPACE;
					}

					i = 0;
					if (p1->tags != NULL) {
						while(p1->tags[i] >= 0) {
							trans->tags[i] = p1->tags[i];
							i++;
						}
					}

					l = i;
					j = 0;
					if (p2->tags != NULL) {
						while (p2->tags[j] >= 0) {
							/* Don't add duplicates. */
							dup = 0;
							for (k = 0; k < i; k++) {
								if (trans->tags[k] == p2->tags[j]) {
									dup = 1;
									break;
								}
							}

							if (!dup) {
								trans->tags[l++] = p2->tags[j];
							}

							j++;
						}
					}
					trans->tags[l] = -1;
				}

				p2++;
			}
			p1++;
		}
	} else {
		/* Compute a maximum limit for the number of transitions leaving
		   from each state. */
		while (p1->position >= 0) {
			p2 = orig_p2;
			while (p2->position >= 0) {
				counts[p1->position]++;
				p2++;
			}
			p1++;
		}
	}

	return REG_OK;
}

/* Converts the syntax tree to a TNFA.All the transitions in the TNFA are
   labelled with one character range (there are no transitions on empty
   strings).  The TNFA takes O(n^2) space in the worst case, `n' is size of
   the regexp. */
static reg_errcode_t tre_ast_to_tnfa(tre_ast_node_t *node, tre_tnfa_transition_t *transitions,
		int *counts, int *offs)
{
	tre_union_t      *uni;
	tre_catenation_t *cat;
	tre_iteration_t  *iter;
	reg_errcode_t     errcode = REG_OK;

	switch (node->type) {
		case LITERAL:
			break;

		case UNION:
			uni     = (tre_union_t *)node->obj;
			errcode = tre_ast_to_tnfa(uni->left, transitions, counts, offs);
			if (errcode != REG_OK) {
				return errcode;
			}
			errcode = tre_ast_to_tnfa(uni->right, transitions, counts, offs);
			break;

		case CATENATION:
			cat     = (tre_catenation_t *)node->obj;
			/* Add a transition from each position in cat->left->lastpos
			   to each position in cat->right->firstpos. */
			errcode = tre_make_trans(cat->left->lastpos, cat->right->firstpos, transitions, counts, offs);
			if (errcode != REG_OK) {
				return errcode;
			}

			errcode = tre_ast_to_tnfa(cat->left, transitions, counts, offs);
			if (errcode != REG_OK) {
				return errcode;
			}

			errcode = tre_ast_to_tnfa(cat->right, transitions, counts, offs);
			break;

		case ITERATION:
			iter = (tre_iteration_t *)node->obj;

			if (iter->max == -1) {
				/* Add a transition from each last position in the iterated
				   expression to each first position. */
				errcode = tre_make_trans(iter->arg->lastpos, iter->arg->firstpos, transitions, counts, offs);
				if (errcode != REG_OK) {
					return errcode;
				}
			}

			errcode = tre_ast_to_tnfa(iter->arg, transitions, counts, offs);
			break;
	}

	return errcode;
}


#define ERROR_EXIT(err) \
	do \
{ \
	errcode = err; \
	if (/*CONSTCOND*/1) \
	goto error_exit; \
} \
while (/*CONSTCOND*/0)


reg_errcode_t mregcomp(regex_t *preg, const char *regex, regex_flags_t cflags)
{
	tre_stack_t           *stack;
	tre_ast_node_t        *tree;
	tre_ast_node_t        *tmp_ast_l;
	tre_ast_node_t        *tmp_ast_r;
	tre_pos_and_tags_t    *p;
	int                   *counts = NULL;
	int                   *offs = NULL;
	int                    i;
	int                    add = 0;
	tre_tnfa_transition_t *transitions;
	tre_tnfa_transition_t *initial;
	tre_tnfa_t            *tnfa = NULL;
	tre_submatch_data_t   *submatch_data;
	tre_tag_direction_t   *tag_directions = NULL;
	reg_errcode_t          errcode;
	tre_mem_t              mem;

	/* Parse context. */
	tre_parse_ctx_t parse_ctx;

	/* Allocate a stack used throughout the compilation process for various
	   purposes. */
	stack = tre_stack_new(512, 1024000, 128);
	if (!stack)
		return REG_ESPACE;

	/* Allocate a fast memory allocator. */
	mem = tre_mem_new();
	if (!mem) {
		tre_stack_destroy(stack);
		return REG_ESPACE;
	}

	/* Parse the regexp. */
	M_mem_set(&parse_ctx, 0, sizeof(parse_ctx));
	parse_ctx.mem    = mem;
	parse_ctx.stack  = stack;
	parse_ctx.start  = regex;
	parse_ctx.cflags = cflags;
	errcode          = tre_parse(&parse_ctx);
	if (errcode != REG_OK)
		ERROR_EXIT(errcode);

	preg->re_nsub = (size_t)parse_ctx.submatch_id - 1;
	tree          = parse_ctx.n;

	/* Allocate the TNFA struct. */
	tnfa = M_malloc_zero(sizeof(tre_tnfa_t));
	if (tnfa == NULL)
		ERROR_EXIT(REG_ESPACE);

	tnfa->num_submatches = (unsigned int)parse_ctx.submatch_id;

	/* Set up tags for submatch addressing.
	 * Figure out how many tags we will need. */
	errcode = tre_add_tags(NULL, stack, tree, tnfa);
	if (errcode != REG_OK) {
		ERROR_EXIT(errcode);
	}

	if (tnfa->num_tags > 0) {
		tag_directions = M_malloc(sizeof(*tag_directions) * ((size_t)tnfa->num_tags + 1));
		if (tag_directions == NULL) {
			ERROR_EXIT(REG_ESPACE);
		}

		tnfa->tag_directions = tag_directions;
		M_mem_set(tag_directions, -1, sizeof(*tag_directions) * ((size_t)tnfa->num_tags + 1));
	}

	tnfa->minimal_tags = M_malloc_zero(((size_t)tnfa->num_tags * 2 + 1) * sizeof(*tnfa->minimal_tags));
	if (tnfa->minimal_tags == NULL) {
		ERROR_EXIT(REG_ESPACE);
	}

	submatch_data = M_malloc_zero(((size_t)parse_ctx.submatch_id) * sizeof(*submatch_data));
	if (submatch_data == NULL) {
		ERROR_EXIT(REG_ESPACE);
	}
	tnfa->submatch_data = submatch_data;

	errcode = tre_add_tags(mem, stack, tree, tnfa);
	if (errcode != REG_OK) {
		ERROR_EXIT(errcode);
	}

	/* Expand iteration nodes. */
	errcode = tre_expand_ast(mem, stack, tree, &parse_ctx.position, tag_directions);
	if (errcode != REG_OK)
		ERROR_EXIT(errcode);

	/* Add a dummy node for the final state. */
	tmp_ast_l = tree;
	tmp_ast_r = tre_ast_new_literal(mem, 0, 0, parse_ctx.position++);
	if (tmp_ast_r == NULL)
		ERROR_EXIT(REG_ESPACE);

	tree = tre_ast_new_catenation(mem, tmp_ast_l, tmp_ast_r);
	if (tree == NULL)
		ERROR_EXIT(REG_ESPACE);

	errcode = tre_compute_nfl(mem, stack, tree);
	if (errcode != REG_OK)
		ERROR_EXIT(errcode);

	counts = M_malloc(sizeof(int) * (size_t)parse_ctx.position);
	offs   = M_malloc(sizeof(int) * (size_t)parse_ctx.position);

	for (i = 0; i < parse_ctx.position; i++) {
		counts[i] = 0;
	}

	tre_ast_to_tnfa(tree, NULL, counts, NULL);

	add = 0;
	for (i = 0; i < parse_ctx.position; i++) {
		offs[i]    = add;
		add       += counts[i] + 1;
		counts[i]  = 0;
	}

	transitions           = M_malloc_zero(((size_t)add + 1) * sizeof(*transitions));
	tnfa->transitions     = transitions;
	tnfa->num_transitions = (unsigned int)add;

	errcode = tre_ast_to_tnfa(tree, transitions, counts, offs);
	if (errcode != REG_OK)
		ERROR_EXIT(errcode);

	tnfa->firstpos_chars = NULL;

	p = tree->firstpos;
	i = 0;
	while (p->position >= 0) {
		i++;
		p++;
	}

	initial       = M_malloc_zero(((unsigned)i + 1) * sizeof(tre_tnfa_transition_t));
	tnfa->initial = initial;

	i = 0;
	for (p = tree->firstpos; p->position >= 0; p++) {
		initial[i].state    = transitions + offs[p->position];
		initial[i].state_id = p->position;
		initial[i].tags     = NULL;

		/* Copy the arrays p->tags, and p->params, they are allocated
		   from a tre_mem object. */
		if (p->tags) {
			size_t j;

			for (j = 0; p->tags[j] >= 0; j++)
				;

			initial[i].tags = M_malloc(sizeof(*p->tags) * (j + 1));
			if (!initial[i].tags)
				ERROR_EXIT(REG_ESPACE);

			M_mem_copy(initial[i].tags, p->tags, sizeof(*p->tags) * (j + 1));
		}

		initial[i].assertions = p->assertions;
		i++;
	}
	initial[i].state = NULL;

	tnfa->num_transitions = (unsigned int)add;
	tnfa->final_trans     = transitions + offs[tree->lastpos[0].position];
	tnfa->num_states      = parse_ctx.position;
	tnfa->cflags          = parse_ctx.cflags;

	tre_mem_destroy(mem);
	tre_stack_destroy(stack);
	M_free(counts);
	M_free(offs);

	preg->tnfa = (void *)tnfa;
	return REG_OK;

error_exit:
	/* Free everything that was allocated and return the error code. */
	tre_mem_destroy(mem);
	if (stack != NULL)
		tre_stack_destroy(stack);
	if (counts != NULL)
		M_free(counts);
	if (offs != NULL)
		M_free(offs);
	preg->tnfa = (void *)tnfa;
	mregfree(preg);
	return errcode;
}




void mregfree(regex_t *preg)
{
	tre_tnfa_t            *tnfa;
	unsigned int           i;
	tre_tnfa_transition_t *trans;

	tnfa = (void *)preg->tnfa;
	if (tnfa == NULL)
		return;
	preg->tnfa = NULL;

	for (i = 0; i < tnfa->num_transitions; i++) {
		if (tnfa->transitions[i].state) {
			if (tnfa->transitions[i].tags) {
				M_free(tnfa->transitions[i].tags);
			}
		}
	}

	if (tnfa->transitions) {
		M_free(tnfa->transitions);
	}

	if (tnfa->initial) {
		for (trans = tnfa->initial; trans->state; trans++) {
			if (trans->tags) {
				M_free(trans->tags);
			}
		}

		M_free(tnfa->initial);
	}

	if (tnfa->submatch_data) {
		for (i = 0; i < tnfa->num_submatches; i++) {
			if (tnfa->submatch_data[i].parents) {
				M_free(tnfa->submatch_data[i].parents);
			}
		}
		M_free(tnfa->submatch_data);
	}

	if (tnfa->tag_directions)
		M_free(tnfa->tag_directions);
	if (tnfa->firstpos_chars)
		M_free(tnfa->firstpos_chars);
	if (tnfa->minimal_tags)
		M_free(tnfa->minimal_tags);
	M_free(tnfa);
}
