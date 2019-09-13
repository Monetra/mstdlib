cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_net
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

include(InstallDepLibs) # Provides copy_deplibs() and install_deplibs()

set(sources
	m_net.c
	m_net_http_simple.c
)

set_source_files_properties(m_net_http_simple.c PROPERTIES COMPILE_DEFINITIONS NET_HTTP_VERSION="${MSTDLIB_VERSION_STRING}")

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
	PUBLIC  Mstdlib::formats
	        Mstdlib::io
	        Mstdlib::tls
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
	EXPORT_NAME net
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_net)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::net ALIAS ${PROJECT_NAME})
