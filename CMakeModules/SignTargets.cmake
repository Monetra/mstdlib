# SignTargets.cmake
#
# Contains utilities for code-signing executables and shared libraries.
#
# Use sign_targets() to sign targets during the build, and sign_files() to sign files that already exist (usually
# used during a custom install step).
#
# IMPORTANT: code signing of targets may be implemented as a POST_BUILD command, so you can only call sign_targets()
#            on targets that are created in the current directory (won't work on targets in other dirs).
#
# Functions:
# ----------
# sign_targets([... names of targets to sign ...])
#   - note: you can only sign targets you control, you can't sign imported targets. If you need to sign files that
#           you don't build yourself, use the sign_files() command after you've copied the files in question to your
#           build or install directory.
#
# sign_files([... full paths of files to sign ...])
#
# Common options:
# ---------------------------------------
# M_SIGN_DISABLE       - set to true to explicitly disable code-signing. If false, code-signing will be enabled if
#                        required variables are set (M_SIGN_PFXFILE on Windows, or M_SIGN_CERT_NAME on macOS).
#
# Windows-only options:
# ---------------------------------------
# M_SIGN_PFXFILE       - .pfx file to read certification info from (REQUIRED on windows)
# M_SIGN_PASSWORD      - password used to access .pfx file (don't hardcode this value ...)
# M_SIGN_TIMESTAMP_URL - URL to use for timestamping, defaults to http://timestamp.verisign.com/scripts/timestamp.dll
#
# MacOS-only options:
# ---------------------------------------
# M_SIGN_CERT_NAME     - common name (or enough of it to uniquely identify the signing certificate) (REQUIRED on macOS)
#
#   The cert name should match all or part of the "Common Name" field in the certificate (case-sensitive).
#   Examples of certificate names:
#      "iPhone Distribution: Main Street Softworks, Inc."
#      "Mac Developer: John Smith"
#
#   If there is only one certificate loaded on your machine for each type of platform / deployment scenario, you
#   can get away with specifying only the generic part (before the colon). For example, if only one Mac distribution
#   certificate is installed on the machine, setting M_SIGN_CERT_NAME to "Mac Distribution" will find the proper key.
#

function(_check_if_signing_enabled out_enabled)
	set(enabled FALSE)
	get_property(already_warned GLOBAL PROPERTY _internal_sign_targets_already_warned)
	
	if (M_SIGN_DISABLE AND (WIN32 OR APPLE))
		if (NOT already_warned)
			message(STATUS "Code signing manually disabled, use -DM_SIGN_DISABLE=FALSE to turn auto-enable back on")
			set_property(GLOBAL PROPERTY _internal_sign_targets_already_warned TRUE)
		endif ()
	elseif (WIN32)
		if (M_SIGN_PFXFILE)
			set(enabled TRUE)
		elseif (NOT already_warned)
			message(STATUS "Code signing disabled, specify a *.pfx file with -DM_SIGN_PFXFILE=... to enable")
			set_property(GLOBAL PROPERTY _internal_sign_targets_already_warned TRUE)
		endif ()
	elseif (APPLE)
		if (M_SIGN_CERT_NAME)
			set(enabled TRUE)
		elseif (NOT already_warned)
			message(STATUS "Code signing disabled, specify certificate name from your keychain with -DM_SIGN_CERT_NAME=... to enable")
			set_property(GLOBAL PROPERTY _internal_sign_targets_already_warned TRUE)
		endif ()
	endif ()
	
	set(${out_enabled} ${enabled} PARENT_SCOPE)
endfunction()


function(_macos_make_sign_command out_cmd)
	find_program(CODESIGN NAMES codesign)
	if (NOT CODESIGN)
		message(FATAL_ERROR "Cannot sign code, could not find 'codesign' executable")
	endif ()
	
	set(${out_cmd} "${CODESIGN}" -s "${M_SIGN_CERT_NAME}" PARENT_SCOPE)
endfunction()


function(_win32_make_sign_command out_cmd out_using_signtool)
	set(${out_cmd} "" PARENT_SCOPE)

	# Set default value for timestamp URL.
	if (NOT DEFINED M_SIGN_TIMESTAMP_URL)
		set(M_SIGN_TIMESTAMP_URL http://timestamp.verisign.com/scripts/timestamp.dll)
	endif ()

	if (NOT EXISTS "${M_SIGN_PFXFILE}")
		message(FATAL_ERROR "Cannot sign code, given PFX file in M_SIGN_PFXFILE (\"${M_SIGN_PFXFILE}\") does not exist")
	endif ()

	# Try to find a signtool utility.
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(arch x64)
	else ()
		set(arch x86)
	endif ()

	# First, try to find signtool.exe from the Windows SDK (since this tool is officially supported by MS).
	# -- the registry checks are for MinGW - when using VS, signtool.exe is guaranteed to be on the path.
	find_program(SIGNTOOL
		NAMES signtool
		PATHS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
		      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot81]"
			  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot]"
			  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]"
			  "$ENV{MSSdk}" # old path for Windows XP / 2003 (windev)
			  "$ENV{ProgramFiles}/Microsoft SDKs/Windows/v6.0" # old path for Windows XP / 2003 (windev)
		PATH_SUFFIXES bin/${arch}
		              bin
	)

	if (NOT SIGNTOOL)
		find_program(OSSLSIGNCODE osslsigncode)

		if (NOT OSSLSIGNCODE)
			message(FATAL_ERROR "Cannot sign code, could not find signtool or osslsigncode executables (need one of them)")
		endif ()
	endif ()

	# Create the signing command we want to use, store in 'cmd'.
	if (SIGNTOOL)
		# signtool.exe sign /f [.pfx file] -p [password] -t [timestamp URL]
		set(cmd "${SIGNTOOL}" sign /q /f "${M_SIGN_PFXFILE}")
		if (M_SIGN_PASSWORD)
			list(APPEND cmd /p "${M_SIGN_PASSWORD}")
		endif ()
		if (M_SIGN_TIMESTAMP_URL)
			list(APPEND cmd /t "${M_SIGN_TIMESTAMP_URL}")
		endif ()
		set(using_signtool TRUE)
	else ()
		# osslsigncode -pkcs12 [.pfx file] -pass [password] -t [timestamp URL] -in [src file] -out [dest file]
		#
		# Long version: https://quotidian-ennui.github.io/blog/2013/06/07/signing-windows-installers-on-linux/
		# The long version isn't necessary in current osslsigncode (1.7.1), but I'll leave the link here just in
		# case we need to use an old version for some reason down the road.
		#
		set(cmd "${OSSLSIGNCODE}" -pkcs12 "${M_SIGN_PFXFILE}")
		if (M_SIGN_PASSWORD)
			list(APPEND cmd -pass "${M_SIGN_PASSWORD}")
		endif ()
		if (M_SIGN_TIMESTAMP_URL)
			list(APPEND cmd -t "${M_SIGN_TIMESTAMP_URL}")
		endif ()
		set(using_signtool FALSE)
	endif ()
	
	set(${out_cmd}            "${cmd}"            PARENT_SCOPE)
	set(${out_using_signtool} "${using_signtool}" PARENT_SCOPE)
endfunction()


# Helper for sign_targets_*
function(_is_target_signable target out_varname)
	set(${out_varname} FALSE PARENT_SCOPE)

	# Target must have already been defined to be signable.
	if (NOT TARGET ${target})
		message(FATAL_ERROR "Can't sign target ${target}, it's either not a CMake target, or it hasn't been defined yet")
	endif ()

	# Imported targets are not signable. Silently ignore these.
	get_target_property(is_imported ${target} IMPORTED)
	if (is_imported)
		return()
	endif ()

	# Any target type other than executable, module or shared library is not signable. Silently ignore these.
	get_target_property(type ${target} TYPE)
	if (NOT type STREQUAL "SHARED_LIBRARY" AND
		NOT type STREQUAL "MODULE_LIBRARY" AND
		NOT type STREQUAL "EXECUTABLE")
		return()
	endif ()

	set(${out_varname} TRUE PARENT_SCOPE)
endfunction()


# sign_targets_win32([... names of targets to sign ...])
function(sign_targets_win32)
	# Verify that signing is enabled. If it's not, return silently without doing anything.
	_check_if_signing_enabled(enabled)
	if (NOT enabled)
		return ()
	endif ()

	# Make windows sign command string.
	_win32_make_sign_command(cmd using_signtool)

	# Add a post-build command to each signable target that signs it in-place.
	foreach(target ${ARGN})
		_is_target_signable(${target} is_signable)
		if (NOT is_signable)
			continue()
		endif ()

		if (using_signtool)
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND  ${cmd} "$<TARGET_FILE:${target}>"
				VERBATIM
			)
		else ()
			# Note: annoyingly, osslsigncode won't let us use the same file for -in and -out (it causes a crash),
			#       so we have to write to a temp file and then manually overwrite the original file instead.
			#
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND  ${cmd} -in "$<TARGET_FILE:${target}>" -out ${target}_signed_tmp
				COMMAND  ${CMAKE_COMMAND} rename ${target}_signed_tmp "$<TARGET_FILE:${target}>"
				VERBATIM
			)
		endif ()
	endforeach()
endfunction()


# sign_files_win32([... paths of files to sign ...])
function(sign_files_win32)
	# Verify that signing is enabled. If it's not, return silently without doing anything.
	_check_if_signing_enabled(enabled)
	if (NOT enabled)
		return ()
	endif ()

	# Make windows sign command string.
	_win32_make_sign_command(cmd using_signtool)

	foreach(path ${ARGN})
		if (NOT EXISTS "${path}")
			message(FATAL_ERROR "Can't sign ${path}, no file exists at that path.")
		endif ()

		if (using_signtool)
			execute_process(COMMAND ${cmd} "${path}" RESULT_VARIABLE res)
			if (NOT res EQUAL 0)
				message(FATAL_ERROR "Can't sign ${path}, command '${cmd}' failed")
			endif ()
		else ()
			# Note: annoyingly, osslsigncode won't let us use the same file for -in and -out (it causes a crash),
			#       so we have to write to a temp file and then manually overwrite the original file instead.
			#
			get_filename_component(sign_dir  "${path}" DIRECTORY)
			get_filename_component(sign_file "${path}" NAME)

			execute_process(COMMAND ${cmd} -in "${sign_file}" -out "${sign_file}.signed.tmp"
				WORKING_DIRECTORY "${sign_dir}"
				RESULT_VARIABLE   res
			)
			if (NOT res EQUAL 0)
				message(FATAL_ERROR "Can't sign ${path}, command '${cmd}' failed")
			endif ()

			file(RENAME "${sign_dir}/${sign_file}.signed.tmp" "${sign_dir}/${sign_file}")
		endif ()
	endforeach()
endfunction()


# sign_targets_macos([... names of targets to sign ...])
function(sign_targets_macos)
	# Verify that signing is enabled. If it's not, return silently without doing anything.
	_check_if_signing_enabled(enabled)
	if (NOT enabled)
		return ()
	endif ()
	
	_macos_make_sign_command(cmd)
	
	# Add a post-build command to each signable target that signs it in-place.
	foreach(target ${ARGN})
		_is_target_signable(${target} is_signable)
		if (NOT is_signable)
			continue()
		endif ()

		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${cmd} "$<TARGET_FILE:${target}>"
			VERBATIM
		)
	endforeach()
endfunction()


# sign_files_macos([... paths of files to sign ...])
function(sign_files_macos)
	# Verify that signing is enabled. If it's not, return silently without doing anything.
	_check_if_signing_enabled(enabled)
	if (NOT enabled)
		return ()
	endif ()

	_macos_make_sign_command(cmd)

	foreach(path ${ARGN})
		if (NOT EXISTS "${path}")
			message(FATAL_ERROR "Can't sign ${path}, no file exists at that path.")
		endif ()

		execute_process(COMMAND ${cmd} "${path}" RESULT_VARIABLE res)
		if (NOT res EQUAL 0)
			message(FATAL_ERROR "Can't sign ${path}, command '${cmd}' failed")
		endif ()
	endforeach()
endfunction()


# sign_targets([... names of targets to sign ...])
function(sign_targets)
	if (WIN32)
		sign_targets_win32(${ARGN})
	elseif (APPLE)
		sign_targets_macos(${ARGN})
	endif ()
endfunction()


# sign_files([... paths of files to sign ...])
function(sign_files)
	if (WIN32)
		sign_files_win32(${ARGN})
	elseif (APPLE)
		sign_files_macos(${ARGN})
	endif ()
endfunction()

