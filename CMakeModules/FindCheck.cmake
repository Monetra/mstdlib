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
#  Check_FOUND         - System has check
#  Check_INCLUDE_DIRS  - The check include directories
#  Check_LIBRARIES     - The libraries needed to use check
#


find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	PKG_CHECK_MODULES(PC_LIBCHECK QUIET check)
endif ()

set(Check_PATH_LOCATIONS
	/usr/local
	/usr/local/check
	/usr/local/check64
)

find_path(Check_INCLUDE_DIR
	NAMES         check.h
	
	HINTS         "${Check_DIR}"
	              "$ENV{Check_DIR}"
	              "${Check_ROOT_DIR}"
	              "${PC_LIBCHECK_INCLUDE_DIRS}"
	
	PATHS         ${Check_PATH_LOCATIONS}
	
	PATH_SUFFIXES include
                  check
                  include/check
)
mark_as_advanced(FORCE Check_INCLUDE_DIR)

find_library(Check_LIBRARY
	NAMES         check_pic
                  check
	NAMES_PER_DIR

	HINTS         "${Check_DIR}"
	              "$ENV{Check_DIR}"
	              "${Check_ROOT_DIR}"
	              "${PC_LIBCHECK_LIBRARY_DIRS}"

	PATHS         ${Check_PATH_LOCATIONS}

	PATH_SUFFIXES lib
	              ""
)
mark_as_advanced(FORCE Check_LIBRARY)

find_library(Check_SUBUNIT_LIBRARY
	NAMES         subunit
	
	HINTS         "${Check_DIR}"
	              "$ENV{Check_DIR}"
	              "${Check_ROOT_DIR}"
	              "${PC_LIBCHECK_LIBRARY_DIRS}"

	PATHS         ${Check_PATH_LOCATIONS}

	PATH_SUFFIXES lib
	              ""
)
mark_as_advanced(FORCE Check_SUBUNIT_LIBRARY)

if (WIN32)
	find_library(Check_COMPAT_LIBRARY
		NAMES         compat
		
		HINTS         "${Check_DIR}"
		              "$ENV{Check_DIR}"
		              "${Check_ROOT_DIR}"

		PATH_SUFFIXES lib
		              ""
	)
	mark_as_advanced(FORCE Check_COMPAT_LIBRARY)
endif ()

# set Check_FOUND to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Check DEFAULT_MSG
	Check_INCLUDE_DIR
	Check_LIBRARY
)

if (Check_FOUND)
	# Create import libraries.
	#   Check::compat: this one shouldn't be used directly, it's a link dependency of Check::check on some platforms.
	if (NOT TARGET Check::compat AND Check_COMPAT_LIBRARY)
		add_library(Check::compat UNKNOWN IMPORTED)
		set_target_properties(Check::compat PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION                 "${Check_COMPAT_LIBRARY}"
		)
	endif ()

	#   Check::subunit: this one shouldn't be used directly, it's a link dependency of Check::check on some platforms.
	if (NOT TARGET Check::subunit AND Check_SUBUNIT_LIBRARY)
		add_library(Check::subunit UNKNOWN IMPORTED)
		set_target_properties(Check::subunit PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION                 "${Check_SUBUNIT_LIBRARY}"
		)
	endif ()

	#   Check::check: this is the main library that should be linked against externally.
	if (NOT TARGET Check::check AND Check_LIBRARY AND Check_INCLUDE_DIR)
		add_library(Check::check UNKNOWN IMPORTED)
		set_target_properties(Check::check PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES     "${Check_INCLUDE_DIR}"
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION                 "${Check_LIBRARY}"
		)
		if (TARGET Check::compat)
			# Tell consumers to link to Check::compat along with Check::check.
			set_target_properties(Check::check PROPERTIES
				INTERFACE_LINK_LIBRARIES Check::compat
			)
		endif ()
		if (TARGET Check::subunit)
			# Tell consumers to link to Check::subunit along with Check::check.
			set_target_properties(Check::check PROPERTIES
				INTERFACE_LINK_LIBRARIES Check::subunit
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
	set(Check_INCLUDE_DIRS ${Check_INCLUDE_DIR})
	set(Check_LIBRARIES ${Check_LIBRARY})
	if (Check_COMPAT_LIBRARY)
		list(APPEND Check_LIBRARIES ${Check_COMPAT_LIBRARY})
	endif ()
	if (Check_SUBUNIT_LIBRARY)
		list(APPEND Check_LIBRARIES ${Check_SUBUNIT_LIBRARY})
	endif ()
endif ()
