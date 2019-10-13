#ifndef _REGEX_H
#define _REGEX_H

#include <mstdlib/mstdlib.h>

typedef ssize_t regoff_t;

typedef struct re_pattern_buffer {
	size_t  re_nsub;
	void   *tnfa;
} regex_t;

typedef struct {
	regoff_t rm_so;
	regoff_t rm_eo;
} regmatch_t;

typedef enum {
	REG_NONE      = 0,
	REG_ICASE     = 1 << 0,
	REG_MULTILINE = 1 << 1,
	REG_DOTALL    = 1 << 2,
	REG_UNGREEDY  = 1 << 3
} regex_flags_t;

typedef enum {
	REG_OK = 0,
	REG_NOMATCH,
	REG_BADPAT,
	REG_ECOLLATE,
	REG_ECTYPE,
	REG_EESCAPE,
	REG_EBRACK,
	REG_EPAREN,
	REG_EBRACE,
	REG_BADBR,
	REG_ERANGE,
	REG_ESPACE,
	REG_BADRPT,
} reg_errcode_t;

reg_errcode_t mregcomp(regex_t *__restrict, const char *__restrict, regex_flags_t);
reg_errcode_t mregexec(const regex_t *__restrict, const char *__restrict, size_t, regmatch_t *__restrict);
void mregfree(regex_t *);

#endif
