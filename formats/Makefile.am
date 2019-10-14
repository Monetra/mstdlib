lib_LTLIBRARIES = libmstdlib_formats.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/base -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_formats_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_formats_la_SOURCES =  \
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
	json/m_json.c \
	json/m_json_jsonpath.c       \
	json/m_json_reader.c         \
	json/m_json_writer.c         \
	\
	settings/m_settings.c        \
	settings/m_settings_reader.c \
	settings/m_settings_writer.c \
	\
	time/m_mtzfile.c             \
	\
	xml/m_xml.c                  \
	xml/m_xml_entities.c         \
	xml/m_xml_reader.c           \
	xml/m_xml_writer.c           \
	xml/m_xml_xpath.c

libmstdlib_formats_la_DEPENDENCIES = @ADD_OBJECTS@
libmstdlib_formats_la_LIBADD = @ADD_OBJECTS@ $(top_builddir)/base/libmstdlib.la $(top_builddir)/base/libmstdlib_text.la
