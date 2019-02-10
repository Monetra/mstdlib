# Code to help us make Visual Studio compatible libraries when building with MinGW.
#
# Targets passed into convert_mingw_implibs_to_vs() must be defined in the same
# CMakeLists.txt file as the call to the function. This is because the function adds
# a post-build step to do the implib conversion, and post-build steps can't be added
# outside the directory the target is defined in.
#
# If we're not building with MinGW, or if we can't find a visual studio installation,
# convert_mingw_implibs_to_vs() is a no-op.
#
# The convert_mingw_implibs_to_vs() function only works on shared libraries or
# executables that have the "ENABLE_EXPORTS" property set to true. All other targets
# are silently ignored if they're passed in.
#
# Function signature:
#   convert_mingw_implibs_to_vs( ... list of targets ... )
#
include(CygwinPaths)

if (MINGW)
	# -- Find Visual Studio lib.exe, in order to make VS-compatible import libs. --
	set(vcroot)

	# First, try to use vswhere.exe, if we can find it (installed with VS 2017 15.2 and newer).
	set(pf "$ENV{ProgramFiles}")
	set(pf86 "$ENV{ProgramFiles\(x86\)}")
	find_program(VSWHERE NAMES vswhere
		PATHS "${pf}/Microsoft Visual Studio/Installer"
		      "${pf86}/Microsoft Visual Studio/Installer"
	)
	if (VSWHERE)
		execute_process(COMMAND ${VSWHERE} -latest -nologo
			RESULT_VARIABLE res
			OUTPUT_VARIABLE out
			ERROR_QUIET
		)
		if (res EQUAL 0 AND out MATCHES "installationPath:[ \t]*([^\r\n]+)")
			string(STRIP "${CMAKE_MATCH_1}" vcroot)
			if (CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN")
				convert_windows_path(vcroot)
			endif ()
		endif ()
	endif ()

	# If vswhere didn't work, try querying the registry.
	if (NOT vcroot)
		execute_process(COMMAND reg query HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VS7 /reg:32
			RESULT_VARIABLE res
			OUTPUT_VARIABLE out
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if (res EQUAL 0 AND out)
			string(REGEX REPLACE "[\r\n]+" ";" lines "${out}")
			set(latest_ver 0)
			foreach(line ${lines})
				string(STRIP "${line}" line)
				if (line MATCHES "([0-9][0-9.]*)[ \t]+REG_SZ[ \t]+(.+)")
					if (CMAKE_MATCH_1 AND CMAKE_MATCH_2 AND CMAKE_MATCH_1 VERSION_GREATER latest_ver)
						# Remove trailing slashes (if any), convert to cygwin path if we're compiling on that platform.
						string(REGEX REPLACE "[\\]+$" ""  CMAKE_MATCH_2 "${CMAKE_MATCH_2}")
						if (CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN")
							convert_windows_path(CMAKE_MATCH_2)
						endif ()
						if (EXISTS "${CMAKE_MATCH_2}")
							set(latest_ver "${CMAKE_MATCH_1}")
							set(vcroot     "${CMAKE_MATCH_2}")
						endif ()
					endif ()
				endif ()
			endforeach()
		endif ()
	endif ()

	# Try looking for the Windows DDK instead
	if (NOT vcroot)
		set(searchpath "C:/WinDDK/*.*.*")
		if (CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN")
			convert_windows_path(searchpath)
		endif ()

		message("VCRoot not found, searching for DDK instead in ${searchpath}...")

		file(GLOB dirs "${searchpath}")
		if (dirs)
			set(paths)
			list(REVERSE dirs) # put higher version numbers first in list
			foreach(dir ${dirs})

				list(APPEND paths
					"${dir}/bin"
				)
			endforeach()
			message("DDK Search Path ${paths}")
			find_program(DDK_SETENV NAMES setenv.bat PATHS ${paths})
		endif ()

		if (DDK_SETENV)
			message("DDK setenv found at ${DDK_SETENV}")
			get_filename_component(vcroot "${DDK_SETENV}" DIRECTORY)
		endif()
	endif ()

	# If we found vcroot, try to find binary directory containing lib.exe.
	if (vcroot)
		if (CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(arch    x86)
			set(archdir)
		else ()
			set(arch    x64)
			set(archdir /x86_amd64)
		endif ()

		set(paths)
		if (EXISTS "${vcroot}/VC/bin")
			# Old style path:
			list(APPEND paths
				"${vcroot}/VC/bin${archdir}"
				"${vcroot}/VC/bin/amd64"
			)
		elseif (EXISTS "${vcroot}/VC/Tools/MSVC")
			# New style path (VS 2017)
			file(GLOB dirs "${vcroot}/VC/Tools/MSVC/*.*.*")
			list(REVERSE dirs) #put higher version numbers first in the list.
			foreach(dir ${dirs})
				list(APPEND paths
					"${dir}/bin/Hostx86/${arch}"
					"${dir}/bin/Hostx64/${arch}"
				)
			endforeach()
		else ()
			# Windows DDK
			if (CMAKE_SIZEOF_VOID_P EQUAL 4)
				list(APPEND paths
					"${vcroot}/x86"
					"${vcroot}/x86/x86"
				)
			else ()
				list(APPEND paths
					"${vcroot}/amd64"
					"${vcroot}/x64"
					"${vcroot}/x86/amd64"
					"${vcroot}/x86/x64"
				)
			endif()
		endif ()

		find_program(MSVC_LIB NAMES lib
			PATHS ${paths}
		)
		find_program(MSVC_LINK NAMES link
			PATHS ${paths}
			NO_DEFAULT_PATH
		)

		set(MSVC_TOOL_DIR)
		set(MSVC_LIB_ARGS)

		# Newer versions of VC may not include lib.exe, instead you call link.exe /lib
		if (MSVC_LINK AND NOT MSVC_LIB)
			message("Call link.exe /lib instead of lib.exe")
			set(MSVC_LIB_ARGS "/lib" CACHE INTERNAL "" FORCE)
			set(MSVC_LIB "${MSVC_LINK}" CACHE PATH "" FORCE)
		endif()

		if (MSVC_LIB)
			get_filename_component(MSVC_TOOL_DIR "${MSVC_LIB}" DIRECTORY)
		endif ()

		# Path to template file - have to set it outside the function, because it's relative to this file,
		# not the file that's calling the install_exports() function.
		set(_int_mingw_compat_import_lib_script "${CMAKE_CURRENT_LIST_DIR}/MingwCompatImportLibScript.cmake.in")

		if (CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN")
			set(win_tool_dir "${MSVC_TOOL_DIR}")
			set(win_lib      "${MSVC_LIB}")
			set(win_ide      "${vcroot}/Common7/IDE")

			convert_cygwin_path(win_tool_dir)
			convert_cygwin_path(win_lib)
			convert_cygwin_path(win_ide)
			string(REPLACE "/" "\\" win_tool_dir "${win_tool_dir}")
			string(REPLACE "/" "\\" win_lib      "${win_lib}")
			string(REPLACE "/" "\\" win_ide      "${win_ide}")

			set(_int_mingw_compat_run_lib_exe "${CMAKE_CURRENT_BINARY_DIR}/mingw_compat_run_lib.bat")
			file(WRITE "${_int_mingw_compat_run_lib_exe}"
				"@echo off\r\n"
				"SET PATH=${win_tool_dir};${win_ide};%PATH%\r\n"
				"echo PATH = %PATH%\r\n"
				"echo CURRDIR = %cd%\r\n"
				"echo ARGS = ${MSVC_LIB_ARGS} %*\r\n"
				"echo LIBEXE = ${win_lib}\r\n"
				"\"${win_lib}\" ${MSVC_LIB_ARGS} %*"
			)

			convert_cygwin_path(_int_mingw_compat_run_lib_exe)
			string(REPLACE "/" "\\\\" _int_mingw_compat_run_lib_exe "${_int_mingw_compat_run_lib_exe}")
		endif ()
	endif ()
endif ()


function(convert_mingw_implibs_to_vs)
	if (NOT MINGW)
		return()
	endif ()

	set(targets)
	foreach(target ${ARGN})
		get_target_property(imported ${target} IMPORTED)
		if (imported)
			continue()
		endif ()

		get_target_property(type     ${target} TYPE)
		get_target_property(exports  ${target} ENABLE_EXPORTS)
		if (type STREQUAL "SHARED_LIBRARY" OR (type STREQUAL "EXECUTABLE" AND exports))
			list(APPEND targets "${target}")
		endif ()
	endforeach()

	if (NOT targets)
		return()
	endif ()

	if (NOT MSVC_LIB)
		string(REPLACE ";" ", " targets "${targets}")
		message(STATUS "Can't change import libs from MinGW to Visual Studio format for (${targets}): can't find VS tools")
		return()
	endif ()

	set_target_properties(${targets} PROPERTIES
		IMPORT_SUFFIX ".lib"
	)

	foreach(target ${targets})
		set(subdir .)
		if (CMAKE_CONFIGURATION_TYPES)
			set(subdir "$<CONFIG>")
		endif ()
		target_link_libraries(${target} PRIVATE
			"-Wl,--output-def,${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${subdir}/${target}.def"
		)

		set(scriptname "${CMAKE_CURRENT_BINARY_DIR}/${target}_convert_import_lib")

		set(TARGET         ${target})
		set(MSVC_LIB_BATCH "${_int_mingw_compat_run_lib_exe}")
		configure_file("${_int_mingw_compat_import_lib_script}" "${scriptname}.cmake.gen" @ONLY)

		file(GENERATE
			OUTPUT "${scriptname}_$<CONFIG>.cmake"
			INPUT  "${scriptname}.cmake.gen"
		)

		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -P "${scriptname}_$<CONFIG>.cmake"
			VERBATIM
		)
	endforeach()
endfunction()
