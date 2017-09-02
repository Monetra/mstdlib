cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_mysql
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)

option(MSTDLIB_BUILD_SQL_MYSQL "Build MySQL/MariaDB plugin" TRUE)
if (NOT MSTDLIB_BUILD_SQL_MYSQL)
	message(STATUS "Building MSTDLIB MySQL/MariaDB plugin... Disabled")
	return ()
endif ()

find_package(MySQL)
if (NOT TARGET MySQL::mysql)
	# This plugin requires MySQL - if not found and can't be built ourselves, disable it.
	message(STATUS "Building MSTDLIB MySQL/MariaDB plugin... Disabled (requested, but can't be built)")
	return()
endif ()

if (MSTDLIB_SQL_STATIC)
	find_package(OpenSSL)
	if (NOT TARGET OpenSSL::Crypto OR NOT TARGET OpenSSL::SSL)
		message(STATUS "Building MSTDLIB MySQL/MariaDB plugin... Disabled (requested, but can't be built, missing OpenSSL)")
	endif ()
	return ()
endif ()

message(STATUS "Building MSTDLIB MySQL/MariaDB plugin... Enabled")

option(MSTDLIB_INSTALL_SQL_MYSQL "Install third-party MySQL library" FALSE)
if (MSTDLIB_INSTALL_SQL_MYSQL)
	include(InstallDepLibs)
	install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin MySQL::mysql)
endif ()

# Build the library.
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}") # Output module file to bin/, not lib/
endif ()
if (MSTDLIB_SQL_STATIC)
	add_library(${PROJECT_NAME} STATIC
		mstdlib_sql_mysql.c
		mysql_shared.c
	)
	target_link_libraries(${PROJECT_NAME} STATIC
		PRIVATE OpenSSL::Crypto OpenSSL::SSL
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC  MSTDLIB_SQL_STATIC_MYSQL
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
		mstdlib_sql_mysql.c
		mysql_shared.c
	)
endif ()

# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME} PRIVATE
	Mstdlib::thread
	Mstdlib::base
	MySQL::mysql
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
