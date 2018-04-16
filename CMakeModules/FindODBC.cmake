# Find ODBC.
#
# Note that this script does not find IBM DB2 - that database mimics the ODBC API, but it's not an ODBC library.
#
# Set this variable to any additional path you want the module to search:
#  ODBC_DIR
#
# Imported targets defined by this module:
#  ODBC::odbc        - shared library if available, or static if that's all there is
#  ODBC::odbc_shared - shared library
#  ODBC::odbc_static - static library
#
# Informational variables:
#  ODBC_FOUND   - ODBC (or all requested components of ODBC) was found.
#  ODBC_VERSION - the version of ODBC that was found
#

if (WIN32)
	set(ODBC_odbc_FOUND TRUE)
	set(ODBC_odbc_shared_FOUND TRUE)
	if (NOT TARGET ODBC::odbc_shared)
		add_library(ODBC::odbc_shared INTERFACE IMPORTED)
		set_target_properties(ODBC::odbc_shared PROPERTIES
				INTERFACE_LINK_LIBRARIES odbc32
		)
	endif ()
	if (NOT TARGET ODBC::odbc)
		add_library(ODBC::odbc INTERFACE IMPORTED)
		set_target_properties(ODBC::odbc PROPERTIES
			INTERFACE_LINK_LIBRARIES ODBC::odbc_shared
		)
	endif ()
	return()
endif ()

include(CacheLog)
include(FindPackageHandleStandardArgs)

# If ODBC_DIR has changed since the last invocation, wipe internal cache variables so we can search for everything
# again.
if (NOT "${ODBC_DIR}" STREQUAL "${_internal_old_odbc_dir}")
	unset_cachelog_entries()
endif ()
reset_cachelog()

set(_old_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")

find_path(ODBC_INCLUDE_DIR
	NAMES         sql.h isql.h
	HINTS         ${ODBC_DIR}
	              $ENV{ODBC_DIR}
	PATH_SUFFIXES include include/odbc odbc/include include/iodbc iodbc/include
)
add_to_cachelog(ODBC_INCLUDE_DIR)

if (ODBC_INCLUDE_DIR)
	# Calculate root dir of installation from include dir.
	string(REGEX REPLACE "(/)*[Ii][Nn][Cc][Ll][Uu][Dd][Ee](/)*.*$" "" _root_dir "${ODBC_INCLUDE_DIR}")
	set(ODBC_DIR "${_root_dir}" CACHE PATH "Root directory of ODBC installation" FORCE)
	set(_internal_old_odbc_dir "${ODBC_DIR}" CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE ODBC_DIR)

	# Calculate version from header file (if it's in the headers).
	set(_ver_file "${ODBC_INCLUDE_DIR}/sql.h")
	if (_ver_file)
		file(STRINGS "${_ver_file}" _id_str REGEX "^[ \t]*#[ \t]*define[\t ]+ODBCVER[ \t]+0x[0-9]+")
		if (_id_str MATCHES "^[ \t]*#[ \t]*define[\t ]+ODBCVER[ \t]+0x([0-9]+)")
			math(EXPR _major "${CMAKE_MATCH_1} / 100")
			math(EXPR _minor "( ${CMAKE_MATCH_1} % 100 ) / 10")
			math(EXPR _patch "${CMAKE_MATCH_1} % 10")
			set(ODBC_VERSION "${_major}.${_minor}.${_patch}")
		endif ()
	endif ()
	
	set(_libnames odbc32 iodbc odbc odbcinst)
	set(_lib_path_suffixes lib lib/odbc odbc/lib)
	
	# Find static library.
	if (NOT WIN32)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
		find_library(ODBC_STATIC_LIBRARY
			NAMES         ${_libnames}
			NAMES_PER_DIR
			HINTS         ${ODBC_DIR}
			PATH_SUFFIXES ${_lib_path_suffixes}
			NO_DEFAULT_PATH
		)
		add_to_cachelog(ODBC_STATIC_LIBRARY)
		if (ODBC_STATIC_LIBRARY)
			set(ODBC_odbc_static_FOUND TRUE)
			set(ODBC_odbc_FOUND TRUE)
		endif ()
	endif ()
	
	# Find shared library (will pick up static lib if shared not found).
	set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")
	find_library(ODBC_LIBRARY
		NAMES         ${_libnames}
		NAMES_PER_DIR
		HINTS         ${ODBC_DIR}
		PATH_SUFFIXES ${_lib_path_suffixes}
		NO_DEFAULT_PATH
	)
	add_to_cachelog(ODBC_LIBRARY)
	if (ODBC_LIBRARY AND NOT ODBC_LIBRARY STREQUAL ODBC_STATIC_LIBRARY)
		set(ODBC_odbc_shared_FOUND TRUE)
		set(ODBC_odbc_FOUND TRUE)
	endif ()
endif ()

set(_reqs ODBC_INCLUDE_DIR)
if (NOT ODBC_FIND_COMPONENTS) # If user didn't request any particular component explicitly:
	list(APPEND _reqs ODBC_LIBRARY) # Will contain shared lib, or static lib if no shared lib present
endif ()

find_package_handle_standard_args(ODBC
	REQUIRED_VARS     ${_reqs}
	VERSION_VAR       ODBC_VERSION
	HANDLE_COMPONENTS
	FAIL_MESSAGE      "ODBC not found, try -DODBC_DIR=<path>"
)
add_to_cachelog(FIND_PACKAGE_MESSAGE_DETAILS_ODBC)

# Static library.
if (ODBC_odbc_static_FOUND AND NOT TARGET ODBC::odbc_static)
	add_library(ODBC::odbc_static STATIC IMPORTED)
	set_target_properties(ODBC::odbc_static PROPERTIES
		IMPORTED_LOCATION                 "${ODBC_STATIC_LIBRARY}"
		IMPORTED_LINK_INTERFACE_LANGUAGES "C"
		INTERFACE_INCLUDE_DIRECTORIES     "${ODBC_INCLUDE_DIR}"
	)
	set(_odbc_any ODBC::odbc_static)
endif ()

# Shared library.
if (ODBC_odbc_shared_FOUND AND NOT TARGET ODBC::odbc_shared)
	add_library(ODBC::odbc_shared SHARED IMPORTED)
	set_target_properties(ODBC::odbc_shared PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			INTERFACE_INCLUDE_DIRECTORIES     "${ODBC_INCLUDE_DIR}"
	)
	if (WIN32)
		set_target_properties(ODBC::odbc_shared PROPERTIES
			IMPORTED_IMPLIB "${ODBC_LIBRARY}"
		)
		if (ODBC_DLL_LIBRARY)
			set_target_properties(ODBC::odbc_shared PROPERTIES
				IMPORTED_LOCATION "${ODBC_DLL_LIBRARY}"
			)
		endif ()
	else ()
		set_target_properties(ODBC::odbc_shared PROPERTIES
			IMPORTED_LOCATION "${ODBC_LIBRARY}"
		)
	endif ()
	set(_odbc_any ODBC::odbc_shared)
endif ()

# I-don't-care library (shared, or static if shared not available).
if (ODBC_odbc_FOUND AND NOT TARGET ODBC::odbc)
	add_library(ODBC::odbc INTERFACE IMPORTED)
	set_target_properties(ODBC::odbc PROPERTIES
		INTERFACE_LINK_LIBRARIES ${_odbc_any}
	)
endif ()

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")
