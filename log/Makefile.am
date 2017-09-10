lib_LTLIBRARIES = libmstdlib_log.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/log -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_log_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_log_la_SOURCES = \
	m_async_writer.c \
	m_log_android.c \
	m_log.c \
	m_log_common.c \
	m_log_file.c \
	m_log_membuf.c \
	m_log_nslog.c \
	m_log_stream.c \
	m_log_syslog.c \
	m_log_tcp_syslog.c

libmstdlib_log_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_log_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/io/libmstdlib_io.la
