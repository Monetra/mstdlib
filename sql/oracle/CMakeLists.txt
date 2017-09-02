cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_oracle
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)

option(MSTDLIB_BUILD_SQL_ORACLE "Build Oracle plugin" TRUE)
if (NOT MSTDLIB_BUILD_SQL_ORACLE)
	message(STATUS "Building MSTDLIB Oracle plugin... Disabled")
	return ()
endif ()

find_package(Oracle)
if (NOT TARGET Oracle::oci)
	# This plugin requires Oracle - if not found and can't be built ourselves, disable it.
	message(STATUS "Building MSTDLIB Oracle plugin... Disabled (requested, but can't be built)")
	return()
endif ()

message(STATUS "Building MSTDLIB Oracle plugin... Enabled")

option(MSTDLIB_INSTALL_SQL_ORACLE "Install third-party Oracle library" FALSE)
if (MSTDLIB_INSTALL_SQL_ORACLE)
	include(InstallDepLibs)
	install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin Oracle::oci ${Oracle_EXTRA_DEPS})
endif ()

# Build the library.
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}") # Output module file to bin/, not lib/
endif ()
if (MSTDLIB_SQL_STATIC)
	add_library(${PROJECT_NAME} STATIC
		mstdlib_sql_oracle.c
		oracle_shared.c
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC  MSTDLIB_SQL_STATIC_ORACLE
		PRIVATE MSTDLIB_SQL_STATIC_MODULE
	)
	if ((NOT MSTDLIB_STATIC) OR MSTDLIB_STATIC_PIC)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)
	endif ()
	# Add library name to parent directory's list of statically compiled plugins.
	list(APPEND SQL_STATIC_PLUGINS ${PROJECT_NAME})
	set(SQL_STATIC_PLUGINS ${SQL_STATIC_PLUGINS} PARENT_SCOPE)
else ()
	add_library(${PROJECT_NAME} MODULE
		mstdlib_sql_oracle.c
		oracle_shared.c
	)
endif ()

# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME} PRIVATE
	Mstdlib::thread
	Mstdlib::base
	Oracle::oci
)

# Link to main SQL module, if both mstdlib and sql are built shared, or if both are built statically.
if ((MSTDLIB_STATIC AND MSTDLIB_SQL_STATIC) OR (NOT MSTDLIB_STATIC AND NOT MSTDLIB_SQL_STATIC))
	target_link_libraries(${PROJECT_NAME} PRIVATE
		Mstdlib::sql
	)
endif ()

# Make sure there is no "lib" prefix on modules.
set_target_properties(${PROJECT_NAME} PROPERTIES
	PREFIX        ""
	IMPORT_PREFIX ""
)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${mod_dest})
endif ()
