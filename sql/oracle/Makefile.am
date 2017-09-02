lib_LTLIBRARIES = mstdlib_sql_oracle.la
AM_CFLAGS   = -I$(top_srcdir)/include -I$(top_srcdir)/sql
AM_CPPFLAGS = $(AM_CFLAGS)

mstdlib_sql_oracle_la_SOURCES      = mstdlib_sql_oracle.c oracle_shared.c
mstdlib_sql_oracle_la_CFLAGS       = @ORACLE_CPPFLAGS@
mstdlib_sql_oracle_la_LIBADD       = @ADD_OBJECTS@ @ORACLE_LIBADD@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/sql/libmstdlib_sql.la
mstdlib_sql_oracle_la_LDFLAGS      = -module -avoid-version @ORACLE_LDFLAGS@ -export-dynamic
mstdlib_sql_oracle_la_DEPENDENCIES = @ADD_OBJECTS@


