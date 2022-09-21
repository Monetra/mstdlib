lib_LTLIBRARIES = libmstdlib_formats.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/base -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_formats_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_formats_la_SOURCES =  \
	conf/m_conf.c                \
	\
	csv/m_csv.c                  \
	\
	ini/m_ini.c                  \
	ini/m_ini_element.c          \
	ini/m_ini_elements.c         \
	ini/m_ini_kvs.c              \
	ini/m_ini_merger.c           \
	ini/m_ini_reader.c           \
	ini/m_ini_settings.c         \
	ini/m_ini_writer.c           \
	\
	http/m_http.c                \
	http/m_http_body.c           \
	http/m_http_chunk.c          \
	http/m_http_header.c         \
	http/m_http_headers.c        \
	http/m_http_reader.c         \
	http/m_http_simple_reader.c  \
	http/m_http_simple_writer.c  \
	http/m_http_uri.c            \
	\
	json/m_json.c \
	json/m_json_jsonpath.c       \
	json/m_json_reader.c         \
	json/m_json_writer.c         \
	\
	settings/m_settings.c        \
	settings/m_settings_reader.c \
	settings/m_settings_writer.c \
	\
	table/m_table.c              \
	table/m_table_csv.c          \
	table/m_table_json.c         \
	table/m_table_markdown.c     \
	\
	time/m_mtzfile.c             \
	\
	xml/m_xml.c                  \
	xml/m_xml_entities.c         \
	xml/m_xml_reader.c           \
	xml/m_xml_writer.c           \
	xml/m_xml_xpath.c            \
	\
	url/m_url.c                  \
	url/url_parser.c             \


libmstdlib_formats_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_formats_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/text/libmstdlib_text.la
