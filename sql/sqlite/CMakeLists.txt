cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql_sqlite
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)

option(MSTDLIB_BUILD_SQL_SQLITE "Build SQLite plugin" TRUE)
if (NOT MSTDLIB_BUILD_SQL_SQLITE)
	message(STATUS "Building MSTDLIB SQLite plugin... Disabled")
	return ()
endif ()

# Find or build SQLite.
if (EXISTS "${MSTDLIB_TOPLEVEL_DIR}/thirdparty/sqlite-amalgamation/sqlite3.c")
	if (NOT TARGET SQLite::sqlite)
		set(sqlite_dir "${MSTDLIB_TOPLEVEL_DIR}/thirdparty/sqlite-amalgamation")
		add_library(internal_sqlite OBJECT
			"${sqlite_dir}/sqlite3.c"
		)
		target_include_directories(internal_sqlite
			PUBLIC "${sqlite_dir}"
		)
		# Recommended compile time options from: http://sqlite.org/compile.html
		target_compile_definitions(internal_sqlite
			PRIVATE SQLITE_MUTEX_NOOP=1
			        SQLITE_THREADSAFE=1
			        SQLITE_DEFAULT_MEMSTATUS=0
			        SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
			        SQLITE_LIKE_DOESNT_MATCH_BLOBS=1
			        SQLITE_MAX_EXPR_DEPTH=0
			        SQLITE_OMIT_PROGRESS_CALLBACK=1

			PUBLIC  SQLITE_OMIT_DEPRECATED=1
		)
		set_target_properties(internal_sqlite PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)

		# Explicitly suppress all warnings for internal_sqlite only.
		remove_all_warnings_from_targets(internal_sqlite) #see EnableWarnings.cmake

		# Note: need an interface library shim to properly link against the object lib.
		# TODO: get remove this once CMake gets full support for object libs.
		add_library(internal_sqlite_iface INTERFACE)
		target_sources(internal_sqlite_iface
			INTERFACE $<TARGET_OBJECTS:internal_sqlite>
		)
		target_include_directories(internal_sqlite_iface
			INTERFACE "${sqlite_dir}"
		)
		target_compile_definitions(internal_sqlite_iface
			INTERFACE SQLITE_OMIT_DEPRECATED=1
		)

		add_library(SQLite::sqlite ALIAS internal_sqlite_iface)
	endif ()
else ()
	find_package(SQLite)
	if (NOT TARGET SQLite::sqlite)
		# This plugin requires SQLite - if not found and can't be built ourselves, disable it.
		message(STATUS "Building MSTDLIB SQLite plugin... Disabled (requested, but can't be built)")
		return()
	endif ()

	# Note: only need to install SQLite as a third-party dep if we're not building it ourselves.
	option(MSTDLIB_INSTALL_SQL_SQLITE "Install third-party SQLite library" FALSE)
	if (MSTDLIB_INSTALL_SQL_SQLITE)
		include(InstallDepLibs)
		install_deplibs("${MSTDLIB_INSTALL_LOCATION_LIBS}" bin SQLite::sqlite)
	endif ()
endif ()

message(STATUS "Building MSTDLIB SQLite plugin... Enabled")

# Build the library.
if (WIN32)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}") # Output module file to bin/, not lib/
endif ()
if (MSTDLIB_SQL_STATIC)
	add_library(${PROJECT_NAME} STATIC
		mstdlib_sql_sqlite.c
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC  MSTDLIB_SQL_STATIC_SQLITE
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
		mstdlib_sql_sqlite.c
	)
endif ()

# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME} PRIVATE
	Mstdlib::thread
	Mstdlib::base
	SQLite::sqlite
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
