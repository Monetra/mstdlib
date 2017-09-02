# Set a default build type (if not already set by user), and set tooltip text and a drop-down
# list of build types for cmake-gui.
#
# This module also strictly enforces the build type to only be from the list of build_options set below
# (not case sensitive).
#
# For the build-type default to be applied on the initial CMake run in a fresh build directory,
# this module needs to be included before the project() command.
#


# Include guard - only include this script if it hasn't already been included in a parent CMakeLists.txt file.
if (_internal_default_build_type_cmake_included)
	return()
endif ()
set(_internal_default_build_type_cmake_included TRUE)


# Define what build options we want to allow, as well as the default if no choice is made by user.
set(build_options
	Release
	Debug
	MinSizeRel
	RelWithDebInfo
)

set(default_build_type
	Release
)

string(REPLACE ";" ", " build_options_str "${build_options}")


# Make sure the chosen build type is in the list of options.
if (CMAKE_BUILD_TYPE)
	string(TOLOWER "${CMAKE_BUILD_TYPE}" _type_lower)
	set(_ok FALSE)
	foreach(_opt ${build_options})
		string(TOLOWER "${_opt}" _opt_lower)
		if ("${_opt_lower}" STREQUAL "${_type_lower}")
			set(CMAKE_BUILD_TYPE "${_opt}")
			set(_ok TRUE)
			break()
		endif ()
	endforeach()
	
	if (NOT _ok)
		message(WARNING	"CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}' is invalid, using '${default_build_type}' instead.")
		set(CMAKE_BUILD_TYPE ${default_build_type})
	endif ()
endif ()


# Set default option (if not already set) and help text for CMAKE_BUILD_TYPE.
set(doc_string "Choose the type of build, options are: ${build_options_str}")
if (CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}   CACHE STRING "${doc_string}" FORCE)
else()
	set(CMAKE_BUILD_TYPE ${default_build_type} CACHE STRING "${doc_string}" FORCE)
endif ()

# Make CMAKE_BUILD_TYPE a drop-down box in cmake-gui.
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
	STRINGS ${build_options}
)

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode.")

