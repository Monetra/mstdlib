cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_net
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

include(InstallDepLibs) # Provides copy_deplibs() and install_deplibs()

find_package(OpenSSL REQUIRED)

set(sources
	m_net.c
	m_net_http_simple.c
	smtp/m_net_smtp.c
	smtp/m_flow_process.c
	smtp/m_flow_tcp.c
	smtp/m_flow_tcp_sendmsg.c
	smtp/m_flow_tcp_starttls.c
	smtp/m_flow_tcp_auth.c
	smtp/m_flow_tcp_ehlo.c
	smtp/m_flow_tcp_smtp_response.c
	smtp/m_net_smtp_endpoint.c
	smtp/m_net_smtp_session.c
	smtp/m_net_smtp_queue.c
)

set_source_files_properties(m_net_http_simple.c PROPERTIES COMPILE_DEFINITIONS NET_HTTP_VERSION="${MSTDLIB_VERSION_STRING}")
set_source_files_properties(m_net_smtp.c PROPERTIES COMPILE_DEFINITIONS NET_SMTP_VERSION="${MSTDLIB_VERSION_STRING}")

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
	PRIVATE OpenSSL::Crypto Mstdlib::config
)

# Link extra system libraries.
if (WIN32 OR MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE ws2_32.lib crypt32.lib iphlpapi.lib
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

	# If we're not being chain-built, install OpenSSL as dep libs.
	if (CMAKE_SOURCE_DIR STREQUAL Mstdlib_SOURCE_DIR)
		install_deplibs(${MSTDLIB_INSTALL_LOCATION_LIBS} bin
			OpenSSL::Crypto
		)
	endif ()

endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::net ALIAS ${PROJECT_NAME})
