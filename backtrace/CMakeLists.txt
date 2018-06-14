cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_backtrace
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

include(InstallDepLibs) # Provides copy_deplibs() and install_deplibs()

set(Backtrace_LIBRARIES "")
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" AND HAVE_EXECINFO_H)
	find_package(Backtrace REQUIRED)
endif ()


set(sources
	m_backtrace.c
)

if (WIN32 OR MINGW)
	list(APPEND sources
		m_backtrace_win.c
	)
	SET(Backtrace_LIBRARIES imagehlp dbghelp)
else ()
	list(APPEND sources
		m_backtrace_unx.c
	)
endif ()

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
	PUBLIC  Mstdlib::base
	PRIVATE Mstdlib::config
		${Backtrace_LIBRARIES}
)

# Versioning on the library.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION   ${PROJECT_VERSION}
	SOVERSION ${PROJECT_SOVERSION}
	EXPORT_NAME backtrace
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_tls)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::backtrace ALIAS ${PROJECT_NAME})

