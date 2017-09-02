lib_LTLIBRARIES = mstdlib_sql_odbc.la
AM_CFLAGS   = -I$(top_srcdir)/include -I$(top_srcdir)/sql -I$(top_srcdir)/sql/mysql -I$(top_srcdir)/sql/postgresql -I$(top_srcdir)/sql/oracle
AM_CPPFLAGS = $(AM_CFLAGS)

mstdlib_sql_odbc_la_SOURCES      = mstdlib_sql_odbc.c odbc_db2.c odbc_mssql.c ../mysql/mysql_shared.c ../postgresql/postgresql_shared.c ../oracle/oracle_shared.c
mstdlib_sql_odbc_la_CFLAGS       = @ODBC_CPPFLAGS@
mstdlib_sql_odbc_la_LIBADD       = @ADD_OBJECTS@ @ODBC_LIBADD@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/thread/libmstdlib_thread.la $(top_builddir)/sql/libmstdlib_sql.la
mstdlib_sql_odbc_la_LDFLAGS      = -module -avoid-version @ODBC_LDFLAGS@ -export-dynamic
mstdlib_sql_odbc_la_DEPENDENCIES = @ADD_OBJECTS@


