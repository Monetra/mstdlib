cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_formats
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})


set(sources
	# conf:
	conf/m_conf.c

	# csv:
	csv/m_csv.c

	# email:
	email/m_email.c
	email/m_email_common.c
	email/m_email_simple_helpers.c
	email/m_email_simple_reader.c
	email/m_email_simple_writer.c
	email/m_email_reader.c

	# ini:
	ini/m_ini.c
	ini/m_ini_element.c
	ini/m_ini_elements.c
	ini/m_ini_int.h
	ini/m_ini_kvs.c
	ini/m_ini_merger.c
	ini/m_ini_reader.c
	ini/m_ini_settings.c
	ini/m_ini_writer.c

	# http:
	http/m_http.c
	http/m_http_body.c
	http/m_http_chunk.c
	http/m_http_header.c
	http/m_http_headers.c
	http/m_http_int.h
	http/m_http_reader.c
	http/m_http_reader_int.h
	http/m_http_simple_reader.c
	http/m_http_simple_writer.c
	http/m_http_uri.c

	# json:
	json/m_json.c
	json/m_json_int.h
	json/m_json_jsonpath.c
	json/m_json_reader.c
	json/m_json_writer.c

	# settings:
	settings/m_settings.c
	settings/m_settings_reader.c
	settings/m_settings_writer.c

	# table:
	table/m_table.c
	table/m_table_csv.c
	table/m_table_json.c
	table/m_table_markdown.c

	# time:
	time/m_mtzfile.c

	# xml:
	xml/m_xml.c
	xml/m_xml_entities.c
	xml/m_xml_entities.h
	xml/m_xml_reader.c
	xml/m_xml_writer.c
	xml/m_xml_xpath.c

	# url:
	url/m_url.c
	url/thirdparty/url_parser.c
)

# Build the library.
if (MSTDLIB_STATIC)
	add_library(${PROJECT_NAME} STATIC
		${sources}
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
else ()
	add_library(${PROJECT_NAME} SHARED
		${sources}
	)
endif ()

# Add any extra include directories.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link against any dependencies from other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::text
	PRIVATE Mstdlib::config
)

# Versioning on the library.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION   ${PROJECT_VERSION}
	SOVERSION ${PROJECT_SOVERSION}
	EXPORT_NAME formats
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_formats)


# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::formats ALIAS ${PROJECT_NAME})
