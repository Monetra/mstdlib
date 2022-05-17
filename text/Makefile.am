lib_LTLIBRARIES = libmstdlib_text.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/text -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_text_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_text_la_SOURCES =           \
	re/m_re.c                          \
	re/m_re_match.c                    \
	re/mtre/regcomp.c                  \
	re/mtre/regexec.c                  \
	re/mtre/tre-mem.c                  \
	\
	textcodec/m_textcodec_ascii.c      \
	textcodec/m_textcodec_buffer.c     \
	textcodec/m_textcodec.c            \
	textcodec/m_textcodec_cp037.c      \
	textcodec/m_textcodec_cp1250.c     \
	textcodec/m_textcodec_cp1251.c     \
	textcodec/m_textcodec_cp1252.c     \
	textcodec/m_textcodec_cp1253.c     \
	textcodec/m_textcodec_cp1254.c     \
	textcodec/m_textcodec_cp1255.c     \
	textcodec/m_textcodec_cp1256.c     \
	textcodec/m_textcodec_cp1257.c     \
	textcodec/m_textcodec_cp1258.c     \
	textcodec/m_textcodec_cp500.c      \
	textcodec/m_textcodec_cp874.c      \
	textcodec/m_textcodec_cp_map.c     \
	textcodec/m_textcodec_int.h        \
	textcodec/m_textcodec_iso8859_10.c \
	textcodec/m_textcodec_iso8859_11.c \
	textcodec/m_textcodec_iso8859_13.c \
	textcodec/m_textcodec_iso8859_14.c \
	textcodec/m_textcodec_iso8859_15.c \
	textcodec/m_textcodec_iso8859_16.c \
	textcodec/m_textcodec_iso8859_1.c  \
	textcodec/m_textcodec_iso8859_2.c  \
	textcodec/m_textcodec_iso8859_3.c  \
	textcodec/m_textcodec_iso8859_4.c  \
	textcodec/m_textcodec_iso8859_5.c  \
	textcodec/m_textcodec_iso8859_6.c  \
	textcodec/m_textcodec_iso8859_7.c  \
	textcodec/m_textcodec_iso8859_8.c  \
	textcodec/m_textcodec_iso8859_9.c  \
	textcodec/m_textcodec_percent.c    \
	textcodec/m_textcodec_punycode.c   \
	textcodec/m_textcodec_quoted_printable.c \
	textcodec/m_textcodec_utf8.c \
	\
	verify/m_verify.c

libmstdlib_text_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_text_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la
