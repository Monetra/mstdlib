lib_LTLIBRARIES = libmstdlib_tls.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/tls -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_tls_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_tls_la_SOURCES =  \
	m_tls.c              \
	m_tls_certs.c        \
	m_tls_clientctx.c    \
	m_tls_ctx_common.c   \
	m_tls_hostvalidate.c \
	m_tls_serverctx.c

libmstdlib_tls_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_tls_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/io/libmstdlib_io.la @SSL_LIBS@

if MACOSX
libmstdlib_tls_la_LDFLAGS += -framework Security
endif
