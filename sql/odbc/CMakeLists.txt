cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_odbc
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)

include(InstallDepLibs)    # Provides copy_deplibs() and install_deplibs()
include(ConditionalOption) # Provides conditional_option()

# On windows, make sure module files get output to bin/, not lib/
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

set(mods) # will contain list of target names for each built module


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# ODBC module.
find_package(ODBC)
set(has_deps TRUE)
set(reason)
if (NOT TARGET ODBC::odbc)
	set(has_deps FALSE)
	set(reason "missing deps: ODBC::odbc")
endif ()
conditional_option(MSTDLIB_BUILD_SQL_ODBC ${has_deps} "MSTDLIB ODBC plugin" "${reason}")

if (MSTDLIB_BUILD_SQL_ODBC)
	option(MSTDLIB_INSTALL_SQL_ODBC "Install third-party ODBC library" FALSE)
	if (MSTDLIB_INSTALL_SQL_ODBC)
		install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin ODBC::odbc)
	endif ()

	add_library(mstdlib_sql_odbc MODULE
		mstdlib_sql_odbc.c
	)

	target_link_libraries(mstdlib_sql_odbc PRIVATE
		ODBC::odbc
	)

	list(APPEND mods mstdlib_sql_odbc)
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# DB2 module.
find_package(DB2)
set(has_deps TRUE)
set(reason)
if (NOT TARGET DB2::db2)
	set(has_deps FALSE)
	set(reason "missing deps: DB2::db2")
endif ()
conditional_option(MSTDLIB_BUILD_SQL_DB2 ${has_deps} "MSTDLIB DB2 plugin" "${reason}")

if (MSTDLIB_BUILD_SQL_DB2)
	add_library(mstdlib_sql_db2 MODULE
		mstdlib_sql_odbc.c
	)

	target_link_libraries(mstdlib_sql_db2 PRIVATE
		DB2::db2
	)

	list(APPEND mods mstdlib_sql_db2)
endif ()


# Set properties that are shared between both modules (ODBC and DB2).
if (mods)
	# Add common sources, libs and include dirs to each module.
	foreach (mod ${mods})
		target_sources(${mod} PRIVATE
			../postgresql/postgresql_shared.c
			../oracle/oracle_shared.c
			../mysql/mysql_shared.c
			odbc_mssql.c
			odbc_db2.c
		)
		target_include_directories(${mod} PRIVATE
			../mysql
			../oracle
			../postgresql
		)
		target_link_libraries(${mod} PRIVATE
			Mstdlib::thread
			Mstdlib::base
		)
		# Link to main SQL module, if both mstdlib and sql are built shared, or if both are built statically.
		if ((MSTDLIB_STATIC AND MSTDLIB_SQL_STATIC) OR (NOT MSTDLIB_STATIC AND NOT MSTDLIB_SQL_STATIC))
			target_link_libraries(${mod} PRIVATE
				Mstdlib::sql
			)
		endif ()
	endforeach ()

	# Make sure there is no "lib" prefix on modules.
	set_target_properties(${mods} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)

	# Installation rules.
	if (MSTDLIB_INSTALL_LIBS)
		install(TARGETS ${mods} EXPORT Mstdlib ${mod_dest})
	endif ()
endif ()

