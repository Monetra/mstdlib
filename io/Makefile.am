lib_LTLIBRARIES = libmstdlib_io.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/io @C_ARES_CPPFLAGS@ -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_io_la_LDFLAGS = -export-dynamic -version-info @MSTDLIB_VERSION@ @C_ARES_LDFLAGS@
libmstdlib_io_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_io_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la @C_ARES_LIBADD@

libmstdlib_io_la_SOURCES = \
	m_dns.c \
	m_event.c \
	m_event_timer.c \
	m_event_trigger.c \
	m_io.c \
	m_io_block.c \
	m_io_bluetooth.c \
	m_io_bluetooth_notimpl.c \
	m_io_bwshaping.c \
	m_io_hid.c \
	m_io_loopback.c \
	m_io_net.c \
	m_io_netdns.c \
	m_io_serial.c \
	m_io_trace.c

if WIN32
libmstdlib_io_la_SOURCES +=     \
	m_event_win32.c         \
	m_io_osevent_win32.c    \
	m_io_pipe_win32.c       \
	m_io_serial_win32.c     \
	m_io_w32overlap.c       \
	m_io_win32_common.c
libmstdlib_io_la_LIBADD += Ws2_32.lib winmm.lib Setupapi.lib Hid.lib
else
libmstdlib_io_la_SOURCES +=     \
	m_event_poll.c          \
	m_io_osevent_pipe.c     \
	m_io_pipe_posix.c       \
	m_io_posix_common.c     \
	m_io_serial_posix.c
endif

if HAVE_KQUEUE
libmstdlib_io_la_SOURCES +=     \
        m_event_kqueue.c
endif

if HAVE_EPOLL
libmstdlib_io_la_SOURCES +=     \
	m_event_epoll.c
endif

if LINUX
libmstdlib_io_la_SOURCES +=     \
	m_io_hid_linux.c
else
libmstdlib_io_la_SOURCES +=     \
	m_io_hid_notimpl.c
endif

if MACOSX
libmstdlib_io_la_LDFLAGS += -framework CoreFoundation -framework IOKit
endif


