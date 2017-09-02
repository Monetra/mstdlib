cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_log
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

# Library sources.
set(srcs
	m_async_writer.c
	m_log.c
	m_log_common.c
	m_log_file.c
	m_log_membuf.c
	m_log_stream.c
	m_log_syslog.c
	m_log_tcp_syslog.c
	m_log_android.c
	m_log_nslog.c
)
set(building_nslog_sys)
if (APPLE OR IOS OR IOSSIM)
	find_library(FOUNDATION Foundation REQUIRED)
	list(APPEND srcs
		m_log_nslog_sys.m
	)
	set(building_nslog_sys TRUE)
endif ()

# Build the library.
if (MSTDLIB_STATIC)
	add_library(${PROJECT_NAME} STATIC
		${srcs}
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
	if (${MSTDLIB_STATIC_PIC})
		set_target_properties(${PROJECT_NAME} PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)
	endif ()
else ()
	add_library(${PROJECT_NAME} SHARED
		${srcs}
	)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		POSITION_INDEPENDENT_CODE TRUE
	)
endif ()

# Set include directories.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::io
	PRIVATE Mstdlib::config
)
if (ANDROID)
	# Link against android log library (liblog.so).
	target_link_libraries(${PROJECT_NAME}
		PRIVATE log
	)
endif ()

# NSLog (apple logging system)
if (building_nslog_sys)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE HAVE_NSLOG_SYS
	)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE ${FOUNDATION}
	)
endif ()

# Library versioning.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION     ${PROJECT_VERSION}
	SOVERSION   ${PROJECT_SOVERSION}
	EXPORT_NAME log
)

# Override mingw automatically adding lib as prefix to library name.
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

# Alias target for use from other modules. Name must match import lib returned by FindMstdlib.
add_library(Mstdlib::log ALIAS ${PROJECT_NAME})
