cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_tls
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

find_package(OpenSSL REQUIRED)

# Copy OpenSSL libs to bin directory, so we can run test executables in place.
if (MSTDLIB_BUILD_TESTS)
	include(InstallDepLibs) # Provides copy_deplibs() and install_deplibs()
	copy_deplibs(OpenSSL::SSL OpenSSL::Crypto)
endif ()

set(sources
	m_tls.c
	m_tls_certs.c
	m_tls_clientctx.c
	m_tls_ctx_common.c
	m_tls_hostvalidate.c
	m_tls_serverctx.c
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

# Add current directory as a private include directory, so we can find internal headers.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link dependencies from other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::io
	PRIVATE OpenSSL::SSL OpenSSL::Crypto Mstdlib::config
)

# Link extra system libraries.
if (WIN32 OR MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE Ws2_32.lib Crypt32.lib
	)
endif ()

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
	EXPORT_NAME tls
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()


# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::tls ALIAS ${PROJECT_NAME})
