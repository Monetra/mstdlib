cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_formats
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})


set(sources
	# csv:
	csv/m_csv.c

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

	# time:
	time/m_mtzfile.c

	# verify:
	verify/m_verify.c

	# xml:
	xml/m_xml.c
	xml/m_xml_entities.c
	xml/m_xml_entities.h
	xml/m_xml_reader.c
	xml/m_xml_writer.c
	xml/m_xml_xpath.c
)

# Build the library.
if (MSTDLIB_STATIC)
	add_library(${PROJECT_NAME} STATIC
		${sources}
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
	if (MSTDLIB_STATIC_PIC)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)
	endif ()
else ()
	add_library(${PROJECT_NAME} SHARED
		${sources}
	)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		POSITION_INDEPENDENT_CODE TRUE
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
	PUBLIC  Mstdlib::base
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
