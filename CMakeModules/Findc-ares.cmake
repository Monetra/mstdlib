# Find c-ares.
#
# Set this variable to any additional path you want the module to search:
#  c_ares_DIR
#
# Imported targets defined by this module:
#  c-ares::cares        - shared library if available, or static if that's all there is
#  c-ares::cares_shared - shared library
#  c-ares::cares_static - static library
#
# Informational variables:
#  c_ares_FOUND          - c-ares (or all requested components of c-ares) was found.
#  c_ares_VERSION        - the version of c-ares that was found
#

include(CacheLog)
include(FindPackageHandleStandardArgs)

# If c_ares_DIR has changed since the last invocation, wipe internal cache variables so we can search for everything
# again.
if (NOT "${c_ares_DIR}" STREQUAL "${_internal_old_cares_dir}")
	unset_cachelog_entries()
endif ()
reset_cachelog()

set(_old_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	PKG_CHECK_MODULES(PC_C_ares QUIET c-ares)
endif ()

if (WIN32)
	set(_pref "$ENV{ProgramFiles}/c-ares")
	set(_pref86 "$ENV{ProgramFiles\(x86\)}/c-ares")
	set(_paths
		"${_pref}"
		"${_pref}3"
		"${_pref86}"
		"${_pref86}3"
	)
else ()
	set(_paths
		/usr/local/c-ares
		/usr/local/cares
	)
endif ()

find_path(c_ares_INCLUDE_DIR
	NAMES         ares.h
	HINTS         ${c_ares_DIR}
	              $ENV{c_ares_DIR}
	              ${c_ares_ROOT_DIR}
	              ${PC_C_ares_INCLUDE_DIRS}
	PATHS         ${_paths}
	PATH_SUFFIXES include
)
add_to_cachelog(c_ares_INCLUDE_DIR)

if (c_ares_INCLUDE_DIR)
	# Calculate root directory of installation from include directory.
	string(REGEX REPLACE "(/)*include(/)*$" "" _root_dir "${c_ares_INCLUDE_DIR}")
	set(c_ares_DIR "${_root_dir}" CACHE PATH "Root directory of c-ares installation" FORCE)
	set(_internal_old_cares_dir "${c_ares_DIR}" CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE c_ares_DIR)

	# Find version by parsing ares_version.h header.
	if (EXISTS "${c_ares_INCLUDE_DIR}/ares_version.h")
		file(STRINGS "${c_ares_INCLUDE_DIR}/ares_version.h" c_ares_version_str
			REGEX "^[ \t]*#[ \t]*define[\t ]+ARES_VERSION_STR[ \t]+\".*\"")
		if (c_ares_version_str MATCHES "^[ \t]*#[ \t]*define[\t ]+ARES_VERSION_STR[ \t]+\"([^\"]*)\"")
			set(c_ares_VERSION "${CMAKE_MATCH_1}")
		endif ()
	endif ()

	# Find static library.
	set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
	find_library(c_ares_STATIC_LIBRARY
		NAMES         cares_static cares
		NAMES_PER_DIR
		HINTS         ${c_ares_DIR} ${PC_C_ares_LIBRARY_DIRS}
		PATH_SUFFIXES lib ""
	)
	add_to_cachelog(c_ares_STATIC_LIBRARY)
	if (c_ares_STATIC_LIBRARY)
		set(c_ares_static_FOUND TRUE)
		set(c_ares_cares_FOUND TRUE)
	endif ()

	# Find shared library (will pick up static lib if shared not found).
	set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")
	find_library(c_ares_LIBRARY
		NAMES         cares cares_static
		NAMES_PER_DIR
		HINTS         ${c_ares_DIR} ${PC_C_ares_LIBRARY_DIRS}
		PATH_SUFFIXES lib ""
	)
	add_to_cachelog(c_ares_LIBRARY)
	if (c_ares_LIBRARY AND NOT c_ares_LIBRARY STREQUAL c_ares_STATIC_LIBRARY)
		set(c_ares_cares_shared_FOUND TRUE)
		set(c_ares_cares_FOUND TRUE)
	endif ()
	
	# Look for the DLL.
	if (WIN32)
		set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
		find_library(c_ares_DLL_LIBRARY
			NAMES         cares
			NAMES_PER_DIR
			HINTS         ${c_ares_DIR}
			PATH_SUFFIXES bin lib ""
		)
		add_to_cachelog(c_ares_DLL_LIBRARY)
	endif ()
endif ()

set(_reqs c_ares_INCLUDE_DIR)
if (NOT c_ares_FIND_COMPONENTS) # If user didn't request any particular component explicitly:
	list(APPEND _reqs c_ares_LIBRARY) # Will contain shared lib, or static lib if no shared lib present
endif ()

find_package_handle_standard_args(c-ares
	REQUIRED_VARS     ${_reqs}
	VERSION_VAR       c_ares_VERSION
	HANDLE_COMPONENTS
	FAIL_MESSAGE      "c-ares not found, try -Dc_ares_DIR=<path>"
)

# Static library.
if (c_ares_cares_static_FOUND AND NOT TARGET c-ares::cares_static)
	add_library(c-ares::cares_static STATIC IMPORTED)
	set_target_properties(c-ares::cares_static PROPERTIES
		IMPORTED_LOCATION                 "${c_ares_STATIC_LIBRARY}"
		IMPORTED_LINK_INTERFACE_LANGUAGES "C"
		INTERFACE_INCLUDE_DIRECTORIES     "${c_ares_INCLUDE_DIR}"
	)
	set(_c_ares_any c-ares::cares_static)
endif ()

# Shared library.
if (c_ares_cares_shared_FOUND AND NOT TARGET c-ares::cares_shared)
	add_library(c-ares::cares_shared SHARED IMPORTED)
	set_target_properties(c-ares::cares_shared PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			INTERFACE_INCLUDE_DIRECTORIES     "${c_ares_INCLUDE_DIR}"
	)
	if (WIN32)
		set_target_properties(c-ares::cares_shared PROPERTIES
			IMPORTED_IMPLIB "${c_ares_LIBRARY}"
		)
		if (c_ares_DLL_LIBRARY)
			set_target_properties(c-ares::cares_shared PROPERTIES
				IMPORTED_LOCATION "${c_ares_DLL_LIBRARY}"
			)
		endif ()
	else ()
		set_target_properties(c-ares::cares_shared PROPERTIES
			IMPORTED_LOCATION "${c_ares_LIBRARY}"
		)
	endif ()
	set(_c_ares_any c-ares::cares_shared)
endif ()

# I-don't-care library (shared, or static if shared not available).
if (c_ares_cares_FOUND AND NOT TARGET c-ares::cares)
	add_library(c-ares::cares INTERFACE IMPORTED)
	set_target_properties(c-ares::cares PROPERTIES
		INTERFACE_LINK_LIBRARIES ${_c_ares_any}
	)
endif ()

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")

