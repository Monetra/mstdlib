# Find IBM DB2.
#
# Note that DB2 mimics the ODBC api, but it's not really the same.
# Also, DB2 doesn't include version info in headers (they require a runtime call to get the version), so
# we don't read the version as part of the find.
#
# Set this variable to any additional path you want the module to search:
#  DB2_DIR
#
# Imported targets defined by this module:
#  DB2::db2        - shared library if available, or static if that's all there is
#  DB2::db2_shared - shared library
#  DB2::db2_static - static library
#
# Informational variables:
#  DB2_FOUND       - DB2 (or all requested components of DB2) was found.
#

include(CacheLog)
include(FindPackageHandleStandardArgs)

# If DB2_DIR has changed since the last invocation, wipe internal cache variables so we can search for everything
# again.
if (NOT "${DB2_DIR}" STREQUAL "${_internal_old_db2_dir}")
	unset_cachelog_entries()
endif ()
reset_cachelog()

set(_old_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")

find_path(DB2_INCLUDE_DIR
	NAMES         sqlcli1.h
	HINTS         ${DB2_DIR}
	              $ENV{DB2_DIR}
	PATH_SUFFIXES include include/db2 db2/include
)
add_to_cachelog(DB2_INCLUDE_DIR)

if (DB2_INCLUDE_DIR)
	# Calculate root dir of installation from include dir.
	string(REGEX REPLACE "(/)*[Ii][Nn][Cc][Ll][Uu][Dd][Ee](/)*.*$" "" _root_dir "${DB2_INCLUDE_DIR}")
	set(DB2_DIR "${_root_dir}" CACHE PATH "Root directory of DB2 installation" FORCE)
	set(_internal_old_db2_dir "${DB2_DIR}" CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE DB2_DIR)

	set(_libnames db2)
	set(_lib_path_suffixes lib lib/db2 db2/lib)
	
	# Find static library.
	if (NOT WIN32)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
		foreach(defpath "NO_DEFAULT_PATH" "")
			find_library(DB2_STATIC_LIBRARY
				NAMES         ${_libnames}
				NAMES_PER_DIR
				HINTS         ${DB2_DIR}
				PATH_SUFFIXES ${_lib_path_suffixes}
				${defpath}
			)
			if (DB2_STATIC_LIBRARY)
				break()
			endif ()
		endforeach()
		add_to_cachelog(DB2_STATIC_LIBRARY)
		if (DB2_STATIC_LIBRARY)
			set(DB2_db2_static_FOUND TRUE)
			set(DB2_db2_FOUND TRUE)
		endif ()
	endif ()
	
	# Find shared library (will pick up static lib if shared not found).
	set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")
	foreach(defpath "NO_DEFAULT_PATH" "")
		find_library(DB2_LIBRARY
			NAMES         ${_libnames}
			NAMES_PER_DIR
			HINTS         ${DB2_DIR}
			PATH_SUFFIXES ${_lib_path_suffixes}
			${defpath}
		)
		if (DB2_LIBRARY)
			break()
		endif ()
	endforeach()
	add_to_cachelog(DB2_LIBRARY)
	if (DB2_LIBRARY AND NOT DB2_LIBRARY STREQUAL DB2_STATIC_LIBRARY)
		set(DB2_db2_shared_FOUND TRUE)
		set(DB2_db2_FOUND TRUE)
	endif ()
endif ()

set(_reqs DB2_INCLUDE_DIR)
if (NOT DB2_FIND_COMPONENTS) # If user didn't request any particular component explicitly:
	list(APPEND _reqs DB2_LIBRARY) # Will contain shared lib, or static lib if no shared lib present
endif ()

find_package_handle_standard_args(DB2
	REQUIRED_VARS     ${_reqs}
	HANDLE_COMPONENTS
	FAIL_MESSAGE      "DB2 not found, try -DDB2_DIR=<path>"
)
add_to_cachelog(FIND_PACKAGE_MESSAGE_DETAILS_DB2)

# Static library.
if (DB2_db2_static_FOUND AND NOT TARGET DB2::db2_static)
	add_library(DB2::db2_static STATIC IMPORTED)
	set_target_properties(DB2::db2_static PROPERTIES
		IMPORTED_LOCATION                 "${DB2_STATIC_LIBRARY}"
		IMPORTED_LINK_INTERFACE_LANGUAGES "C"
		INTERFACE_INCLUDE_DIRECTORIES     "${DB2_INCLUDE_DIR}"
		INTERFACE_COMPILE_DEFINITIONS     ODBC_IBMDB2
	)
	set(_db2_any DB2::db2_static)
endif ()

# Shared library.
if (DB2_db2_shared_FOUND AND NOT TARGET DB2::db2_shared)
	add_library(DB2::db2_shared SHARED IMPORTED)
	set_target_properties(DB2::db2_shared PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			INTERFACE_INCLUDE_DIRECTORIES     "${DB2_INCLUDE_DIR}"
			INTERFACE_COMPILE_DEFINITIONS     ODBC_IBMDB2
	)
	if (WIN32)
		set_target_properties(DB2::db2_shared PROPERTIES
			IMPORTED_IMPLIB "${DB2_LIBRARY}"
		)
		if (DB2_DLL_LIBRARY)
			set_target_properties(DB2::db2_shared PROPERTIES
				IMPORTED_LOCATION "${DB2_DLL_LIBRARY}"
			)
		endif ()
	else ()
		set_target_properties(DB2::db2_shared PROPERTIES
			IMPORTED_LOCATION "${DB2_LIBRARY}"
		)
	endif ()
	set(_db2_any DB2::db2_shared)
endif ()

# I-don't-care library (shared, or static if shared not available).
if (DB2_db2_FOUND AND NOT TARGET DB2::db2)
	add_library(DB2::db2 INTERFACE IMPORTED)
	set_target_properties(DB2::db2 PROPERTIES
		INTERFACE_LINK_LIBRARIES ${_db2_any}
	)
endif ()

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")
