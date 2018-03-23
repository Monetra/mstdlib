cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_sql
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})


# Library sources.
set(srcs
	m_module.c
	m_sql_connpool.c
	m_sql_driver_helper.c
	m_sql_error.c
	m_sql_query.c
	m_sql_report.c
	m_sql_stmt.c
	m_sql_stmt_bind.c
	m_sql_stmt_result.c
	m_sql_table.c
	m_sql_trace.c
	m_sql_trans.c
)

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
	PUBLIC  Mstdlib::thread
	        Mstdlib::io
	        Mstdlib::formats
	PRIVATE Mstdlib::config
)

# Link dependencies on system
if (NEED_DL)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE dl
	)
endif ()

# Library versioning.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION     ${PROJECT_VERSION}
	SOVERSION   ${PROJECT_SOVERSION}
	EXPORT_NAME sql
)

# Override mingw automatically adding lib as prefix to library name.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_sql)

# Plugins
option(MSTDLIB_SQL_STATIC "Link SQL plugins statically, instead of dynamically at runtime" ${MSTDLIB_STATIC})
if (NOT MSTDLIB_SQL_STATIC AND MSTDLIB_STATIC AND NOT MSTDLIB_STATIC_PIC)
	message(FATAL_ERROR "Can't make SQL plugins with static mstdlib with PIC disabled: set -DMSTDLIB_STATIC_PIC=TRUE")
endif ()

#   note: this option is primarily used by monetra, it needs the third-party deps for packaging.
set(SQL_STATIC_PLUGINS) #each statically-built plugin will add its library target name to this variable.

add_subdirectory(sqlite)
add_subdirectory(mysql)
add_subdirectory(postgresql)
add_subdirectory(oracle)
add_subdirectory(odbc) # also handles db2 and db2 pase

if (SQL_STATIC_PLUGINS)
	# Link plugins directly into main SQL module, since we're not loading it dynamically at runtime anymore.
	target_link_libraries(${PROJECT_NAME} PRIVATE ${SQL_STATIC_PLUGINS})
endif ()


# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()

# Alias target for use from other modules. Name must match import lib returned by FindMstdlib.
add_library(Mstdlib::sql ALIAS ${PROJECT_NAME})
