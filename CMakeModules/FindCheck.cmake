# Find Check library.
#
#
# Config variables you can set before calling find_package:
#  Check_DIR           - use this alternative search path
#
#
# Imported targets defined by this module (use these):
#  Check::check        - main check library
#
#
# Legacy variable defines (don't use these, use imported targets instead):
#  CHECK_FOUND         - System has check
#  CHECK_INCLUDE_DIRS  - The check include directories
#  CHECK_LIBRARIES     - The libraries needed to use check
#


find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	PKG_CHECK_MODULES(PC_LIBCHECK QUIET check)
endif ()

set(CHECK_PATH_LOCATIONS
	/usr/local
	/usr/local/check
	/usr/local/check64
)

find_path(CHECK_INCLUDE_DIR
	NAMES         check.h
	
	HINTS         "${Check_DIR}"
	              "$ENV{Check_DIR}"
	              "${CHECK_ROOT_DIR}"
				  "${PC_LIBCHECK_INCLUDE_DIRS}"
	
	PATHS         ${CHECK_PATH_LOCATIONS}
	
	PATH_SUFFIXES include
				  check
				  include/check
)
mark_as_advanced(FORCE CHECK_INCLUDE_DIR)

find_library(CHECK_LIBRARY
	NAMES         check_pic
				  check
	NAMES_PER_DIR
	
	HINTS         "${Check_DIR}"
	              "$ENV{Check_DIR}"
	              "${CHECK_ROOT_DIR}"
				  "${PC_LIBCHECK_LIBRARY_DIRS}"
				  
	PATHS         ${CHECK_PATH_LOCATIONS}
	
	PATH_SUFFIXES lib
				  ""
)
mark_as_advanced(FORCE CHECK_LIBRARY)

if (WIN32)
	find_library(CHECK_COMPAT_LIBRARY
		NAMES         compat
		
		HINTS         "${Check_DIR}"
		              "$ENV{Check_DIR}"
		              "${CHECK_ROOT_DIR}"
		
		PATH_SUFFIXES lib
					  ""
	)
	mark_as_advanced(FORCE CHECK_COMPAT_LIBRARY)
endif ()

# set CHECK_FOUND to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(check DEFAULT_MSG
	CHECK_INCLUDE_DIR
	CHECK_LIBRARY
)

if (CHECK_FOUND)
	# Create import libraries.
	#   Check::compat: this one shouldn't be used directly, it's a link dependency of Check::check on some platforms.
	if (NOT TARGET Check::compat AND CHECK_COMPAT_LIBRARY)
		add_library(Check::compat UNKNOWN IMPORTED)
		set_target_properties(Check::compat PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION                 "${CHECK_COMPAT_LIBRARY}"
		)
	endif ()
	#   Check::check: this is the main library that should be linked against externally.
	if (NOT TARGET Check::check AND CHECK_LIBRARY AND CHECK_INCLUDE_DIR)
		add_library(Check::check UNKNOWN IMPORTED)
		set_target_properties(Check::check PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES     "${CHECK_INCLUDE_DIR}"
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION                 "${CHECK_LIBRARY}"
		)
		if (TARGET Check::compat)
			# Tell consumers to link to Check::compat along with Check::check.
			set_target_properties(Check::check PROPERTIES
				INTERFACE_LINK_LIBRARIES Check::compat
			)
		endif ()
		if (CMAKE_SYSTEM_NAME MATCHES "Linux")
			set_target_properties(Check::check PROPERTIES
				INTERFACE_LINK_LIBRARIES -lrt
			)
		endif ()
	endif ()
	
	# Set legacy output variables (don't use these).
	# TODO: remove these once everybody is updated to use import libs.
	set(CHECK_INCLUDE_DIRS ${CHECK_INCLUDE_DIR})
	set(CHECK_LIBRARIES ${CHECK_LIBRARY})
	if (CHECK_COMPAT_LIBRARY)
		list(APPEND CHECK_LIBRARIES ${CHECK_COMPAT_LIBRARY})
	endif ()
endif ()
