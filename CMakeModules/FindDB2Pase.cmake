# Find IBM DB2 Pase (AIX only).
#
# Note that DB2 mimics the ODBC api, but it's not really the same.
# Also, DB2 doesn't include version info in headers (they require a runtime call to get the version), so
# we don't read the version as part of the find.
#
# The PASE module is for DB2 running on OS400. It provides a shared library and export file.
#
# Set this variable to any additional path you want the module to search:
#  DB2Pase_DIR          - additional paths to search for libs
#
# Imported targets defined by this module:
#  DB2Pase::db2        - shared library (sort of, it's AIX compat)
#
# Informational variables:
#  DB2Pase_FOUND   - DB2Pase (or all requested components of DB2Pase) was found.
#

include(CacheLog)
include(FindPackageHandleStandardArgs)

# If DB2_DIR has changed since the last invocation, wipe internal cache variables so we can search for everything
# again.
if (NOT "${DB2Pase_DIR}" STREQUAL "${_internal_old_db2pase_dir}")
	unset_cachelog_entries()
endif ()
reset_cachelog()

find_path(DB2Pase_INCLUDE_DIR
	NAMES         sqlcli.h
	HINTS         ${DB2Pase_DIR}
	              $ENV{DB2Pase_DIR}
	PATH_SUFFIXES include db2_pase include/db2_pase db2_pase/include
)
add_to_cachelog(DB2Pase_INCLUDE_DIR)

if (DB2Pase_INCLUDE_DIR)
	# Calculate root dir of installation from include dir.
	string(REGEX REPLACE "(/)*[Ii][Nn][Cc][Ll][Uu][Dd][Ee](/)*.*$" "" _root_dir "${DB2Pase_INCLUDE_DIR}")
	set(DB2Pase_DIR "${_root_dir}" CACHE PATH "Root directory of DB2 Pase installation" FORCE)
	set(_internal_old_db2pase_dir "${DB2Pase_DIR}" CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE DB2Pase_DIR)

	# Find the import lib.
	find_file(DB2Pase_IMPORT_LIB
		NAMES        libdb400.exp
		HINTS        ${DB2Pase_DIR}
		PATH_SUFFXES lib db2_pase lib/db2_pase db2_pase/lib include include/db2_pase db2_pase/include
	)
	add_to_cachelog(DB2Pase_IMPORT_LIB)
	set(DB2Pase_db2_FOUND TRUE)
endif ()

set(_reqs DB2Pase_INCLUDE_DIR)
if (NOT DB2Pase_FIND_COMPONENTS) # If user didn't request any particular component explicitly:
	list(APPEND _reqs DB2Pase_IMPORT_LIB)
endif ()

find_package_handle_standard_args(DB2Pase
	REQUIRED_VARS     ${_reqs}
	HANDLE_COMPONENTS
	FAIL_MESSAGE      "DB2Pase not found, try -DDB2Pase_DIR=<path>"
)
add_to_cachelog(FIND_PACKAGE_MESSAGE_DETAILS_DB2Pase)

# Import lib.
if (DB2Pase_db2_FOUND AND NOT TARGET DB2Pase::db2)
	add_library(DB2Pase::db2 INTERFACE IMPORTED)
	set_target_properties(DB2Pase::db2 PROPERTIES
			INTERFACE_LINK_LIBRARIES      "-Wl,-bI${DB2Pase_IMPORT_LIB}"
			INTERFACE_INCLUDE_DIRECTORIES "${DB2Pase_INCLUDE_DIR}"
			INTERFACE_COMPILE_DEFINITIONS ODBC_IBMDB2_PASE
	)
endif ()

