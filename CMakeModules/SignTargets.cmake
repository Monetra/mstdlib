# SignTargets.cmake
#
# Contains utilities for code-signing executables and shared libraries.
#
# IMPORTANT: code signing may be implemented as a POST_BUILD command, so you can only call sign_targets()
#            on targets that are created in the current directory (won't work on targets in other dirs).
#
# Functions:
# ----------
# sign_targets([... names of targets to sign ...])
#
# Options that control what sign_targets() does:
# ---------------------------------------
# M_SIGN_PFXFILE       - .pfx file to read certification info from
# M_SIGN_PASSWORD      - password used to access .pfx file (don't hardcode this value ...)
# M_SIGN_TIMESTAMP_URL - URL to use for timestamping, defaults to
#

function(_is_target_signable target out_varname)
	set(${out_varname} FALSE PARENT_SCOPE)

	# Target must have already been defined to be signable.
	if (NOT TARGET ${target})
		return()
	endif ()

	# Imported targets are not signable.
	get_target_property(is_imported ${target} IMPORTED)
	if (is_imported)
		return()
	endif ()

	# Any target type other than executable, module or shared library is not signable.
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
	# Set default value for timestamp URL.
	if (NOT DEFINED M_SIGN_TIMESTAMP_URL)
		set(M_SIGN_TIMESTAMP_URL http://timestamp.verisign.com/scripts/timestamp.dll)
	endif ()

	if (NOT EXISTS "${M_SIGN_PFXFILE}")
		message(STATUS "Code signing disabled, given PFX file in M_SIGN_PFXFILE (\"${M_SIGN_PFXFILE}\") does not exist")
		return()
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
			message(STATUS "Code signing disabled, could not find signtool or osslsigncode executables (need one of them)")
			return()
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
	endif ()

	# Add a post-build command to each signable target that signs it in-place.
	foreach (target ${ARGN})
		_is_target_signable(${target} is_signable)
		if (NOT is_signable)
			continue()
		endif ()

		if (SIGNTOOL)
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
	endforeach ()
endfunction()

# sign_targets([... names of targets to sign ...])
function(sign_targets)
	if (WIN32)
		sign_targets_win32(${ARGN})
	endif ()
	# TODO: add macOS and iOS in here too (if applicable, might not work the same way in Apple-land)
endfunction()
