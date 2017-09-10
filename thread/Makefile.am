lib_LTLIBRARIES = libmstdlib_thread.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/base -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_thread_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_thread_la_SOURCES = \
	m_atomic.c \
	m_popen.c \
	m_thread_attr.c \
	m_thread.c \
	m_thread_coop.c \
	m_threadpool.c \
	m_thread_rwlock_emu.c \
	m_thread_tls.c

if HAVE_PTHREAD
libmstdlib_thread_la_SOURCES += m_thread_pthread.c
endif

if WIN32
libmstdlib_thread_la_SOURCES += m_thread_win.c m_pollemu.c
endif

libmstdlib_thread_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_thread_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la @LIBPTHREAD@
