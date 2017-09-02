# - Try to find the Windows Driver Development Kit (WinDDK).
#
# Set the following for alternative search path:
#  WINDDK_ROOT_DIR 
#
# Once done this will define
#  WinDDK_FOUND - true if the system has WinDDK
#  WINDDK_INCLUDE_DIRS - the WinDDK Include Directories
#  WINDDK_LIBRARY_DIRS - the WinDDK Library Directories

# Cache settings and WinDDK environment variables take precedence,
# or we try to fall back to the default search.

INCLUDE (FindPackageHandleStandardArgs)

function(WINDDK_EXPAND_SUBDIRS var)
	SET(_out)
	SET(_result)
	FOREACH(prefix ${ARGN})
		FILE(GLOB _globbed "${prefix}*/")
		IF(_globbed)
			LIST(SORT _globbed)
			LIST(REVERSE _globbed)
			LIST(APPEND _out ${_globbed})
		ENDIF()
	ENDFOREACH()
	FOREACH(_name ${_out})
		get_filename_component(_name "${_name}" ABSOLUTE)
		IF(IS_DIRECTORY "${_name}")
			LIST(APPEND _result "${_name}")
		ENDIF()
	ENDFOREACH()
	IF (_result)
		LIST (REMOVE_DUPLICATES _result)
	ENDIF ()
	SET(${var} "${_result}" PARENT_SCOPE)
endfunction()

IF (WIN32)
	# Visual Studio >= 2015 and some MinGW have DDK headers built in lets test for working headers, if we
	# have them, then just set WINDDK_LIBRARY_DIRS and WINDDK_INCLUDE_DIRS blank.
	IF (NOT WinDDK_FOUND)
		SET(CMAKE_REQUIRED_LIBRARIES hid.lib)
		CHECK_INCLUDE_FILES ("windows.h;hidsdi.h"  HAVE_HIDSDI_H)
		CHECK_SYMBOL_EXISTS ("HidD_GetAttributes" "windows.h;hidsdi.h" HAVE_HIDD_GETATTRIBUTES)
		SET(CMAKE_REQUIRED_LIBRARIES )

		IF ("${HAVE_HIDSDI_H}" AND "${HAVE_HIDD_GETATTRIBUTES}")
			SET(WINDDK_INCLUDE_DIRS )
			SET(WINDDK_LIBRARY_DIRS )
			SET(WinDDK_FOUND 1)
		ENDIF ()
	ENDIF ()

	IF (NOT (WINDDK_INCLUDE_DIRS AND WINDDK_LIBRARY_DIRS) AND NOT WinDDK_FOUND)
		WINDDK_EXPAND_SUBDIRS(WINDDK_SEARCHPATHS
			"$ENV{SYSTEMDRIVE}/WinDDK/"
			"$ENV{ProgramFiles}/Windows Kits/"
			"$ENV{ProgramFiles\(x86\)}/Windows Kits/"
			"C:/WinDDK"
		)

		find_path(WINDDK_INCLUDE_DIRS
			NAMES hidsdi.h
			PATHS
				"${WINDDK_ROOT_DIR}"
				"$ENV{WINDDK_PATH}"
				"$ENV{WINDDK_ROOT}"
				${WINDDK_SEARCHPATHS}
			PATH_SUFFIXES
				inc/api
				Include/shared
		)

		find_path(WINDDK_INCLUDE_DIRS NAMES hidsdi.h)
		mark_as_advanced(WINDDK_INCLUDE_DIRS)

		LIST(APPEND WINDDK_SYSTEM_NAMES "w2k" "wxp" "wnet" "wlh" "win7" "win8")
		IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
			LIST(APPEND WINDDK_ARCH_NAMES "amd64" "x64")
		ELSE ()
			LIST(APPEND WINDDK_ARCH_NAMES "i386" "x86")
		ENDIF ()

		FOREACH(name ${WINDDK_SYSTEM_NAMES})
			FOREACH(arch ${WINDDK_ARCH_NAMES})
				LIST(APPEND WINDDK_LIBPATH_SUFFIX "lib/${name}/${arch}")
				LIST(APPEND WINDDK_LIBPATH_SUFFIX "lib/${name}/um/${arch}")
			ENDFOREACH()
		ENDFOREACH()

		find_path(WINDDK_LIBRARY_DIRS
			NAMES hid.lib
			PATHS
				"${WINDDK_ROOT_DIR}"
				"$ENV{WINDDK_PATH}"
				"$ENV{WINDDK_ROOT}"
				${WINDDK_SEARCHPATHS}
			PATH_SUFFIXES
				${WINDDK_LIBPATH_SUFFIX}
		)
		mark_as_advanced(WINDDK_LIBRARY_DIRS)
	ENDIF ()

	IF (NOT WinDDK_FOUND)
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(WinDDK
			REQUIRED_VARS WINDDK_INCLUDE_DIRS WINDDK_LIBRARY_DIRS
			FAIL_MESSAGE "WinDDK not found try -DWINDDK_ROOT_DIR=<path>"
		)
	ENDIF ()

ENDIF ()

