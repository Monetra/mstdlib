lib_LTLIBRARIES = libmstdlib_sql.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/sql -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_sql_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_sql_la_SOURCES = \
	m_module.c              \
	m_sql_connpool.c        \
	m_sql_driver_helper.c   \
	m_sql_error.c           \
	m_sql_query.c           \
	m_sql_report.c          \
	m_sql_stmt.c            \
	m_sql_stmt_bind.c       \
	m_sql_stmt_result.c     \
	m_sql_table.c           \
	m_sql_tabledata.c       \
	m_sql_trace.c           \
	m_sql_trans.c

libmstdlib_sql_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_sql_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/io/libmstdlib_io.la @MSTDLIB_SQL_LIBADD@
