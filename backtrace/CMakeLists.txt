cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_backtrace
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})


# Build the library.
set(libtype SHARED)
if (MSTDLIB_STATIC)
	set(libtype STATIC)
endif ()

add_library(${PROJECT_NAME} ${libtype}
	m_backtrace.c
)


# General settings.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::base
	PRIVATE Mstdlib::config
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION     ${PROJECT_VERSION}
	SOVERSION   ${PROJECT_SOVERSION}
	EXPORT_NAME backtrace
)

if (MSTDLIB_STATIC)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
endif ()


# Platform-specific settings.
if (WIN32)
	target_sources(${PROJECT_NAME} PRIVATE
		m_backtrace_win.c
	)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		imagehlp
		dbghelp
	)
else ()
	target_sources(${PROJECT_NAME} PRIVATE
		m_backtrace_unx.c
	)

	if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND HAVE_EXECINFO_H)
		find_package(Backtrace REQUIRED)
		target_link_libraries(${PROJECT_NAME} PRIVATE
			${Backtrace_LIBRARIES}
		)
	endif ()
endif ()

# Visual Studio does not normally prefix libraries with "lib", but it seems MinGW does, override that behavior.
if (MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(${PROJECT_NAME})

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::backtrace ALIAS ${PROJECT_NAME})

