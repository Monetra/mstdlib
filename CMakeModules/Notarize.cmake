# Notarize.cmake
#
# macOS only.
#
# Uploads files to Apple's notarization service.
# Does not handle stapling because that is an out of band process
# that needs to happen well after building has finished.
#
# File type suitable for notarizing are:
# - .dmg
# - .zip
#
# Notarizing a .app file requires the .app to be put into a .zip archive.
# See section on generating zip archives for an example.
#
# Requires code signed packages (CodeSign.cmake). Also, entitlements must be enabled because the hardened
# run time is required for notarization. CodeSign only turns on hardened run time when an entablement file
# is provided because, most likely, hardened run time entitlements need to be configured.
#
# Functions:
# ----------
# notarize_is_enabled(outname)
#   - Checks to see if notarization has been enabled, and the required authentication info has been set. Places result
#     in variable named [out name].
#
# notarize_files([... full paths of files to notarize ...])
#   - Immediately notarizes the given files. Files must already exist on disk when this is called, so this is usually
#     called as part of a custom install script.
#   - If code notarization is disabled, this function is silently ignored.
#
# Common options:
# ---------------
# M_NOTARIZE_DISABLE   - set to true to explicitly disable notarization. If false, notarization will be enabled if
#                        the required variables are set M_NOTARIZE_USERNAME and M_NOTARIZE_PASSWORD
#
# Options:
# --------
# M_NOTARIZE_USERNAME     - Username to use for notarization.
# M_NOTARIZE_PASSWORD     - Password to use for notarization. Should be a reference to a username stored in the key chain.
#                           Key chain reference takes the form "@keychain:<KEY CHAIN ENTRY NAME>". For example, 
#                           "@keychain:AC_PASSWORD". An app specific password is most likely needed.
# M_NOTARIZE_BUNDLE_ID    - Bundle ID to identify the application with the notarization service. Does not have to be
#                           the same as the package bundle id but should be unique to the application. If not present
#                           the file name will be used as the bundle identifier.
# M_NOTARIZE_ASC_PROVIDER - The provider short name to determine which Team to sign using. Only needed when the
#                           notarization account belongs to multiple teams.
#
# Generating zip archive for .app packages
# ----------------------------------------
#
# Use ditto to put the app into a zip archive.
# ditto is part of a standard install of macOS
# so we know it will be present and can be used.
# The notarization docs even use it in their example
# for creating a zip archive.
#
# set(app_dir "<PATH AND FILENAME OF .app>)
# find_program(DITTO NAMES ditto)
# if (DITTO)
# 	get_filename_component(fn "${app_dir}" NAME)
# 	get_filename_component(fn_p "${app_dir}" PATH)
# 	execute_process(COMMAND ${DITTO} -c -k --keepParent "${fn}" "${fn}.zip" WORKING_DIRECTORY "${fn_p}" RESULT_VARIABLE res)
# 	if (res EQUAL 0)
# 		message("\nNotarizing ${fn} ...")
# 		notarize_files("${app_dir}.zip" QUIET)
# 		execute_process(COMMAND rm "${app_dir}.zip")
# 	endif ()
# endif ()
# 
# For details about notarization see
#   https://developer.apple.com/documentation/xcode/notarizing_your_app_before_distribution/customizing_the_notarization_workflow
#

# Include guard.
if (_internal_notarize_already_included)
	return()
endif ()
set(_internal_notarize_already_included TRUE)

# Add empty cache entries for config options, if not set, so that user can see them in cmake-gui.
if (NOT DEFINED M_NOTARIZE_DISABLE)
	option(M_NOTARIZE_DISABLE "Force disable notarization?" FALSE)
endif ()
mark_as_advanced(FORCE M_NOTARIZE_DISABLE)

include(CodeSign)

if (APPLE)
	# Do this on every run
	set(M_NOTARIZE_USERNAME "${M_NOTARIZE_USERNAME}" CACHE STRING "Notarization service username (required for notarization)")
	set(M_NOTARIZE_PASSWORD "${M_NOTARIZE_PASSWORD}" CACHE STRING "Notarization service password (required for notarization)")
endif ()

function(notarize_is_enabled out_enabled)	
	set(enabled FALSE)
	set(quiet   FALSE)
	if ("QUIET" IN_LIST ARGN)
		set(quiet TRUE)
	endif ()

	if (APPLE)
		code_sign_is_enabled(cs_enabled)
		code_sign_entitlements_enabled(cs_entitlements_enabled)

		if (M_NOTARIZE_DISABLE)
			if (NOT quiet)
				message("Notarization manually disabled, use -DM_NOTARIZE_DISABLE=FALSE to turn auto-enable back on")
			endif ()
		elseif (NOT cs_enabled)
			if (NOT quiet)
				message("Notarization disabled, because code sigin is disabled")
			endif ()
		elseif (NOT cs_entitlements_enabled)
			if (NOT quiet)
				message("Notarization disabled, because code sigin does not have entitles set")
			endif ()
		else ()
			if (M_NOTARIZE_USERNAME AND M_NOTARIZE_PASSWORD)
				set(enabled TRUE)
			elseif (NOT quiet)
				message("Notarization disabled, specify username and password with -DM_NOTARIZE_USERNAME=... and -DM_NOTARIZE_PASSWORD=... to enable")
			endif ()
		endif ()
	endif ()

	set(${out_enabled} ${enabled} PARENT_SCOPE)
endfunction()


function(notarize_get_cmd_macos out_cmd bundle_id)
	find_program(XCRUN NAMES xcrun)
	if (NOT XCRUN)
		message(FATAL_ERROR "Cannot notarize, could not find 'xcrun' executable")
	endif ()

	set(cmd "${XCRUN}")
	list(APPEND cmd altool --notarize-app)

	list(APPEND cmd --username "${M_NOTARIZE_USERNAME}")
	list(APPEND cmd --password "${M_NOTARIZE_PASSWORD}")

	if (M_NOTARIZE_BUNDLE_ID)
		list(APPEND cmd --primary-bundle-id "${M_NOTARIZE_BUNDLE_ID}")
	else ()
		string(REGEX REPLACE "[-!@#$%^&*()_+={}|\\:;'\"><,~`[? \n\r\t]|]" "." bundle_id "${bundle_id}")
		list(APPEND cmd --primary-bundle-id "${bundle_id}")
	endif ()

	if (M_NOTARIZE_ASC_PROVIDER)
		list(APPEND cmd --asc-provider "${M_NOTARIZE_ASC_PROVIDER}")
	endif ()

	set(${out_cmd} "${cmd}" PARENT_SCOPE)
endfunction()


# notarize_files_macos([... paths of files to sign ...] [QUIET])
function(notarize_files_macos)
	set(files "${ARGN}")

	set(quiet)
	if ("QUIET" IN_LIST files)
		set(quiet QUIET)
		list(REMOVE_ITEM files "QUIET")
	endif ()

	# Verify that signing is enabled. If it's not, return silently without doing anything.
	notarize_is_enabled(enabled ${quiet})
	if (NOT enabled)
		return ()
	endif ()

	foreach(path ${files})
		get_filename_component(fn "${path}" NAME)
		notarize_get_cmd_macos(cmd ${fn})

		if (NOT quiet)
			message("  Notarizing: ${path}")
		endif ()

		if (NOT EXISTS "${path}")
			message(FATAL_ERROR "Can't notarize ${path}, no file exists at that path.")
		endif ()

		execute_process(COMMAND ${cmd} --file "${path}" RESULT_VARIABLE res ERROR_VARIABLE err)
		if (NOT res EQUAL 0)
			message(FATAL_ERROR "Can't notarize ${path}, command '${cmd}' failed (${res}): ${err}")
		endif ()
	endforeach()
endfunction()


# notarize_files([... paths of files to notarize ...] [QUIET])
function(notarize_files)
	if (APPLE)
		notarize_files_macos(${ARGN})
	endif ()
endfunction()
