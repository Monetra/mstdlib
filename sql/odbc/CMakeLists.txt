cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_odbc
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)

set(mod_strs
	ODBC
	DB2
)
if (CMAKE_SYSTEM_NAME STREQUAL "AIX")
	list(APPEND mod_strs DB2Pase)
endif ()

# Options controlling the various things that can be built in this subdirectory.
foreach (mod_str ${mod_strs})
	option(MSTDLIB_BUILD_SQL_${mod_str} "Build the ${mod_str} plugin" TRUE)

	if (NOT MSTDLIB_BUILD_SQL_${mod_str})
		message(STATUS "Building MSTDLIB ODBC plugin... Disabled")
	endif ()
endforeach ()

# On windows, make sure module files get output to bin/, not lib/
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

set(mods) # will contain list of target names for each built module


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# ODBC module.
if (MSTDLIB_BUILD_SQL_ODBC)
	# This plugin requires ODBC - iODBC, UnixODBC, or Microsoft ODBC on Windows, if not found, disable it.
	find_package(ODBC)
	if (TARGET ODBC::odbc)
		message(STATUS "Building MSTDLIB ODBC plugin... Enabled")

		option(MSTDLIB_INSTALL_SQL_ODBC "Install third-party ODBC library" FALSE)
		if (MSTDLIB_INSTALL_SQL_ODBC)
			include(InstallDepLibs)
			install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin ODBC::odbc)
		endif ()

		add_library(mstdlib_sql_odbc MODULE
			mstdlib_sql_odbc.c
		)

		target_link_libraries(mstdlib_sql_odbc PRIVATE
			ODBC::odbc
		)

		list(APPEND mods mstdlib_sql_odbc)
	else ()
		message(STATUS "Building MSTDLIB ODBC plugin... Disabled (requested, but can't be built)")
	endif ()
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# DB2 module.
if (MSTDLIB_BUILD_SQL_DB2)
	find_package(DB2)
	if (TARGET DB2::db2)
		message(STATUS "Building MSTDLIB DB2 plugin... Enabled")

		add_library(mstdlib_sql_db2 MODULE
			mstdlib_sql_odbc.c
		)

		target_link_libraries(mstdlib_sql_db2 PRIVATE
			DB2::db2
		)

		list(APPEND mods mstdlib_sql_db2)
	else ()
		message(STATUS "Building MSTDLIB DB2 plugin... Disabled (requested, but can't be built)")
	endif ()
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# DB2 Pase module.
if (MSTDLIB_BUILD_SQL_DB2Pase)
	find_package(DB2Pase)
	if (TARGET DB2Pase::db2)
		message(STATUS "Building MSTDLIB DB2Pase plugin... Enabled")

		add_library(mstdlib_sql_db2pase MODULE
			mstdlib_sql_odbc.c
		)

		target_link_libraries(mstdlib_sql_db2pase PRIVATE
			DB2Pase::db2
		)

		list(APPEND mods mstdlib_sql_db2pase)
	else ()
		message(STATUS "Building MSTDLIB DB2Pase plugin... Disabled (requested, but can't be built)")
	endif ()
endif ()


# Set properties that are shared between all three modules (ODBC, DB2 and DB2 Pase).
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
