lib_LTLIBRARIES = mstdlib_sql_sqlite.la
AM_CFLAGS   = -I$(top_srcdir)/include -I$(top_srcdir)/sql
AM_CPPFLAGS = $(AM_CFLAGS)

mstdlib_sql_sqlite_la_SOURCES       = mstdlib_sql_sqlite.c
if SQLITE_AMALGAMATION
mstdlib_sql_sqlite_la_SOURCES      += ../../thirdparty/sqlite-amalgamation/sqlite3.c
endif
mstdlib_sql_sqlite_la_CFLAGS        = @SQLITE_CPPFLAGS@
mstdlib_sql_sqlite_la_LIBADD        = @ADD_OBJECTS@ @SQLITE_LIBADD@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/sql/libmstdlib_sql.la
mstdlib_sql_sqlite_la_LDFLAGS       = -module -avoid-version @SQLITE_LDFLAGS@ -export-dynamic
mstdlib_sql_sqlite_la_DEPENDENCIES  = @ADD_OBJECTS@


