# Find Oracle database client (OCI).
#
# Set this variable to any additional path you want the module to search:
#  Oracle_DIR
#
# Imported targets defined by this module:
#  Oracle::oci - shared library containing the client interface to oracle databases
#
# Informational variables:
#  Oracle_FOUND      - Oracle (or all requested components of Oracle) was found.
#  Oracle_VERSION    - the version of Oracle that was found
#  Oracle_EXTRA_DEPS - extra shared libs that need to be packaged with the main Oracle shared lib

include(CacheLog)
include(FindPackageHandleStandardArgs)

# Helper function for globbing for directories.
function(append_glob_dirs list_name glob_path)
	file(TO_CMAKE_PATH "${glob_path}" glob_path)
	file(GLOB dirs LIST_DIRECTORIES true "${glob_path}")
	if (dirs)
		list(APPEND ${list_name} "${dirs}")
		set(${list_name} "${${list_name}}" PARENT_SCOPE)
	endif ()
endfunction()

# If Oracle_DIR has changed since the last invocation, wipe internal cache variables so we can search for everything
# again.
if (NOT "${Oracle_DIR}" STREQUAL "${_internal_old_oracle_dir}")
	unset_cachelog_entries()
endif ()
reset_cachelog()

set(_old_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")

# Set path guesses.
set(_paths)
if (CMAKE_HOST_WIN32)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(archdir    "$ENV{ProgramFiles}")
		set(notarchdir "$ENV{ProgramFiles\(x86\)}")
	else ()
		set(archdir    "$ENV{ProgramFiles\(x86\)}")
		set(notarchdir "$ENV{ProgramFiles}")
	endif ()
	append_glob_dirs(_paths "${archdir}/Oracle/oci*/")
	append_glob_dirs(_paths "${notarchdir}/Oracle/oci*/")
	append_glob_dirs(_paths "${archdir}/Oracle/*/oci/")
	append_glob_dirs(_paths "${notarchdir}/Oracle/*/oci/")
	append_glob_dirs(_paths "${archdir}/Oracle/instantclient*/")
	append_glob_dirs(_paths "${notarchdir}/Oracle/instantclient*/")
	append_glob_dirs(_paths "${archdir}/instantclient*/")
	append_glob_dirs(_paths "${notarchdir}/instantclient*/")
	append_glob_dirs(_paths "${archdir}/instantclient/*/")
	append_glob_dirs(_paths "${notarchdir}/instantclient/*/")
	append_glob_dirs(_paths "/Oracle/oci*/")
	append_glob_dirs(_paths "/Oracle/*/oci/")
	append_glob_dirs(_paths "/Oracle/instantclient*/")
	append_glob_dirs(_paths "/instantclient*/")
	append_glob_dirs(_paths "/instantclient/*/")
else ()
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(arch 64)
	else ()
		set(arch 32)
	endif ()
	list(APPEND _paths
		/usr/local/oracle${arch}
		/usr/local/oracle
	)
	append_glob_dirs(_paths "/usr/local/oracle*/")
	append_glob_dirs(_paths "/usr/include/oracle/*/")
endif ()

# Find include directory.
find_path(Oracle_INCLUDE_DIR
	NAMES         oci.h
	HINTS         ${Oracle_DIR}
	              $ENV{Oracle_DIR}
	              $ENV{ORACLE_HOME}
	PATHS         ${_paths}
	PATH_SUFFIXES include client64 sdk/include
)
add_to_cachelog(Oracle_INCLUDE_DIR)

if (Oracle_INCLUDE_DIR)
	# Calculate root directory of installation from include directory.
	# If installed from zip file, include path will look like: [...]/installclient_12_2/sdk/include
	# So, to get true root directory, need to parse off everything after include, AND /sdk/ (if present).
	string(REGEX REPLACE "(/)*include(/)*.*$" "" _root_dir "${Oracle_INCLUDE_DIR}")
	string(REGEX REPLACE "(/)*sdk(/)*$" "" _root_dir "${_root_dir}")
	set(Oracle_DIR "${_root_dir}" CACHE PATH "Root directory of Oracle installation" FORCE)
	set(_internal_old_oracle_dir "${Oracle_DIR}" CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE Oracle_DIR)
	
	# Find version by parsing the oci.h header.
	file(STRINGS "${Oracle_INCLUDE_DIR}/oci.h" _vers
		REGEX "^[ \t]*#[ \t]*define[\t ]+OCI_[A-Z]+_VERSION[ \t]+[0-9]+")
	if (_vers MATCHES "[; \t]*#[ \t]*define[\t ]+OCI_MAJOR_VERSION[ \t]+([0-9]+)")
		set(_major ${CMAKE_MATCH_1})
		if (_vers MATCHES "[; \t]*#[ \t]*define[\t ]+OCI_MINOR_VERSION[ \t]+([0-9]+)")
			set(_minor ${CMAKE_MATCH_1})
			set(Oracle_VERSION "${_major}.${_minor}")
		endif ()
	endif ()
	
	# RPM's on Linux do not install the libraries under a common root directory with the includes:
	#  /usr/include/oracle/12.2/client64 -- will contain headers
	#  /usr/lib/oracle/12.2/client64/lib -- will contain .so's
	set(_paths )
	if (Oracle_INCLUDE_DIR MATCHES "usr[/]+include[/]+(oracle[/]+.*)")
		list(APPEND _paths "/usr/lib/${CMAKE_MATCH_1}")
	endif ()
	
	# Find shared library.
	foreach (defpath "NO_DEFAULT_PATH" "")
		find_library(Oracle_LIBRARY
			NAMES         oci clntsh liboci libclntsh
			NAMES_PER_DIR
			HINTS         ${Oracle_DIR}
			PATHS         ${_paths}
			PATH_SUFFIXES lib sdk/lib sdk/lib/msvc
			${defpath}
		)
		if (Oracle_LIBRARY)
			break()
		endif ()
	endforeach()
	add_to_cachelog(Oracle_LIBRARY)
	if (Oracle_LIBRARY)
		set(Oracle_oci_FOUND TRUE)
	endif ()
	
	# Find the DLL (if any).
	if (WIN32)
		set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
		foreach (defpath "NO_DEFAULT_PATH" "")
			find_library(Oracle_DLL_LIBRARY
				NAMES         oci
				NAMES_PER_DIR
				HINTS         ${Oracle_DIR}
				PATH_SUFFIXES bin lib ""
				${defpath}
			)
			if (Oracle_DLL_LIBRARY)
				break()
			endif ()
		endforeach()
		add_to_cachelog(Oracle_DLL_LIBRARY)
	endif ()
endif ()

set(_reqs Oracle_INCLUDE_DIR)
if (NOT Oracle_FIND_COMPONENTS) # If user didn't request any particular component explicitly:
	list(APPEND _reqs Oracle_LIBRARY) # Will contain shared lib
endif ()

find_package_handle_standard_args(Oracle
	REQUIRED_VARS     ${_reqs}
	VERSION_VAR       Oracle_VERSION
	HANDLE_COMPONENTS
	FAIL_MESSAGE      "Oracle not found, try -DOracle_DIR=<path>"
)
add_to_cachelog(FIND_PACKAGE_MESSAGE_DETAILS_Oracle)

# Shared library.
if (Oracle_oci_FOUND AND NOT TARGET Oracle::oci)
	add_library(Oracle::oci SHARED IMPORTED)
	set_target_properties(Oracle::oci PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			INTERFACE_INCLUDE_DIRECTORIES     "${Oracle_INCLUDE_DIR}"
	)
	if (WIN32)
		set_target_properties(Oracle::oci PROPERTIES
			IMPORTED_IMPLIB "${Oracle_LIBRARY}"
		)
		if (Oracle_DLL_LIBRARY)
			set_target_properties(Oracle::oci PROPERTIES
				IMPORTED_LOCATION "${Oracle_DLL_LIBRARY}"
			)
		endif ()
	else ()
		set_target_properties(Oracle::oci PROPERTIES
			IMPORTED_LOCATION "${Oracle_LIBRARY}"
		)
	endif ()
endif ()

set(CMAKE_FIND_LIBRARY_SUFFIXES "${_old_suffixes}")

# Extra lib dependencies.
set(Oracle_EXTRA_DEPS)

function(_oracle_add_extra_dep name)
	find_library(Oracle_DEP_${name}
		NAMES         ${ARGN}
		NAMES_PER_DIR
		HINTS         ${Oracle_DIR}
		PATHS         ${_paths}
		PATH_SUFFIXES lib sdk/lib sdk/lib/msvc
	)
	add_to_cachelog(Oracle_DEP_${name})
	if (Oracle_DEP_${name})
		list(APPEND Oracle_EXTRA_DEPS "${Oracle_DEP_${name}}")
		set(Oracle_EXTRA_DEPS "${Oracle_EXTRA_DEPS}" PARENT_SCOPE)
	endif ()
endfunction()

# Note: Oracle 12 on Windows doesn't have any extra dependencies besides oci.dll.

# Oracle 11 and 12 (linux only):
_oracle_add_extra_dep(NNZ nnz12 libnnz12 nnz11 libnnz11)
# Oracle 12 only (linux only):
_oracle_add_extra_dep(CLNTSHCORE clntshcore libclntshcore)
_oracle_add_extra_dep(MQL1 mql1 libmql1)
_oracle_add_extra_dep(IPC1 ipc1 libipc1)
_oracle_add_extra_dep(ONS ons libons)
