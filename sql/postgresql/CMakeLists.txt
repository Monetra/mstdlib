cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_postgresql
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Setup (third-party deps and options).
include(InstallDepLibs)    # Provides copy_deplibs() and install_deplibs()
include(ConditionalOption) # Provides conditional_option()

# Find third-party deps.
find_package(PostgreSQL)
set(has_deps TRUE)
set(reason)
if (NOT TARGET PostgreSQL::pq)
	set(has_deps FALSE)
	string(APPEND reason " PostgreSQL::pq")
endif ()

if (MSTDLIB_SQL_STATIC)
	find_package(OpenSSL)
	if (NOT TARGET OpenSSL::Crypto OR NOT TARGET OpenSSL::SSL)
		string(APPEND reason " OpenSSL::Crypto OpenSSL::SSL")
		set(has_deps FALSE)
	endif ()
endif ()

if (reason)
	set(reason "missing deps:${reason}")
endif ()

# Provide option to build, dependent on whether or not dependencies are found.
conditional_option(MSTDLIB_BUILD_SQL_POSTGRESQL ${has_deps} "MSTDLIB PostgreSQL plugin" "${reason}")
if (NOT MSTDLIB_BUILD_SQL_POSTGRESQL)
	return ()
endif ()

# If linking statically, need to copy OpenSSL DLL's to bin directory so we can run test executables in place.
if (MSTDLIB_SQL_STATIC AND MSTDLIB_BUILD_TESTS)
	copy_deplibs(OpenSSL::SSL OpenSSL::Crypto)
endif ()

option(MSTDLIB_INSTALL_SQL_POSTGRESQL "Install third-party PostgreSQL library" FALSE)
if (MSTDLIB_INSTALL_SQL_POSTGRESQL)
	install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin PostgreSQL::pq)
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Build and install module.
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}") # Output module file to bin/, not lib/
endif ()
if (MSTDLIB_SQL_STATIC)
	add_library(${PROJECT_NAME} STATIC
		mstdlib_sql_postgresql.c
		postgresql_shared.c
	)
	target_link_libraries(${PROJECT_NAME} STATIC
		PRIVATE OpenSSL::Crypto OpenSSL::SSL
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC  MSTDLIB_SQL_STATIC_POSTGRESQL
		PRIVATE MSTDLIB_SQL_STATIC_MODULE
	)
	# Add library name to parent directory's list of statically compiled plugins.
	list(APPEND SQL_STATIC_PLUGINS ${PROJECT_NAME})
	set(SQL_STATIC_PLUGINS ${SQL_STATIC_PLUGINS} PARENT_SCOPE)
else ()
	add_library(${PROJECT_NAME} MODULE
		mstdlib_sql_postgresql.c
		postgresql_shared.c
	)
endif ()

# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME} PRIVATE
	Mstdlib::thread
	Mstdlib::base
	PostgreSQL::pq
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
