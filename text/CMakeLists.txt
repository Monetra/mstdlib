cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_text
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

include(InstallDepLibs) # Provides copy_deplibs() and install_deplibs()

set(sources
	#re:
	re/m_re.c
	re/m_re_match.c
	re/mtre/regcomp.c
	re/mtre/regexec.c
	re/mtre/tre-mem.c

	# textcodec
	textcodec/m_textcodec.c
	textcodec/m_textcodec_buffer.c
	textcodec/m_textcodec_ascii.c
	textcodec/m_textcodec_cp037.c
	textcodec/m_textcodec_cp500.c
	textcodec/m_textcodec_cp874.c
	textcodec/m_textcodec_cp1250.c
	textcodec/m_textcodec_cp1251.c
	textcodec/m_textcodec_cp1252.c
	textcodec/m_textcodec_cp1253.c
	textcodec/m_textcodec_cp1254.c
	textcodec/m_textcodec_cp1255.c
	textcodec/m_textcodec_cp1256.c
	textcodec/m_textcodec_cp1257.c
	textcodec/m_textcodec_cp1258.c
	textcodec/m_textcodec_cp_map.c
	textcodec/m_textcodec_iso8859_1.c
	textcodec/m_textcodec_iso8859_2.c
	textcodec/m_textcodec_iso8859_3.c
	textcodec/m_textcodec_iso8859_4.c
	textcodec/m_textcodec_iso8859_5.c
	textcodec/m_textcodec_iso8859_6.c
	textcodec/m_textcodec_iso8859_7.c
	textcodec/m_textcodec_iso8859_8.c
	textcodec/m_textcodec_iso8859_9.c
	textcodec/m_textcodec_iso8859_10.c
	textcodec/m_textcodec_iso8859_11.c
	textcodec/m_textcodec_iso8859_13.c
	textcodec/m_textcodec_iso8859_14.c
	textcodec/m_textcodec_iso8859_15.c
	textcodec/m_textcodec_iso8859_16.c
	textcodec/m_textcodec_percent.c
	textcodec/m_textcodec_punycode.c
	textcodec/m_textcodec_quoted_printable.c

	# verify:
	verify/m_verify.c
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

# Add current directory as a private include directory, so we can find internal headers.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link dependencies from other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::base
	PRIVATE Mstdlib::config
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_library(SECURITY NAMES Security)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE "${SECURITY}"
	)
endif ()


# Versioning on the library.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION   ${PROJECT_VERSION}
	SOVERSION ${PROJECT_SOVERSION}
	EXPORT_NAME text
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_text)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::text ALIAS ${PROJECT_NAME})
