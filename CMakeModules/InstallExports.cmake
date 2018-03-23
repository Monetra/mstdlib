# InstallExports.cmake
#
# Helper function for installing export config files ([name]Config.cmake, etc).
#
# Adding dependency names (OpenSSL, ZLIB, etc.) to end of arg list will create exported
# files that call the FindDependencyMacro. To request a particular version, add a colon,
# followed by the version number (e.g., "Mstdlib:1.0.0").
#
# If a COMPATIBILITY value isn't provided, 'SameMajorVersion' is used. See this doc for
# a description of what the COMPATABILITY values mean:
#   https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html#generating-a-package-version-file
#
# Note: Compatibility "SameMinorVersion" is only available as an option with CMake 3.10 or later.
#
# install_exports( <export name>
#   <export installation destination>
#   [COMPATIBILITY <AnyNewerVersion|SameMajorVersion|ExactVersion>]
#   <... variable number of dependencies ...> )
#
# Ex: install_exports(Monetra share/Monetra/cmake)

include(CMakePackageConfigHelpers)

# Path to template file - have to set it outside the function, because it's relative to this file,
# not the file that's calling the install_exports() function.
set(_int_install_exports_template "${CMAKE_CURRENT_LIST_DIR}/InstallExportsScript.cmake.in")

function(install_exports export_name export_dest)
	set(deps "${ARGN}")
	set(compatibility SameMajorVersion)

	# If user specified a COMPATIBILITY value, use that instead of the default.
	list(FIND deps "COMPATIBILITY" idx)
	if (idx GREATER -1)
		list(REMOVE_AT deps ${idx}) # Remove "COMPATIBILITY" marker from deps list.
		list(GET deps ${idx} compatibility)
		list(REMOVE_AT deps ${idx}) # Remove compatibility value from deps list.
	endif ()

	# Generate *Version.cmake file and install it.
	set(ver_file "${CMAKE_CURRENT_BINARY_DIR}/${export_name}ConfigVersion.cmake")

	include(CMakePackageConfigHelpers)
	write_basic_package_version_file("${ver_file}" COMPATIBILITY ${compatibility})

	install(FILES "${ver_file}" DESTINATION "${export_dest}")

	set(config_file ${export_name}Config.cmake)
	if (deps)
		set(targets_file ${export_name}Targets.cmake)
	else ()
		set(targets_file ${config_file})
	endif ()

	# Install *Targets.cmake and *Config-[build type].cmake files. If we don't need to inject find_dependency()
	# commands, just install the *Targets.cmake file as the main *Config.cmake file.
	install(EXPORT ${export_name}
		NAMESPACE   ${export_name}::
		DESTINATION "${export_dest}"
		FILE        ${targets_file}
	)

	if (deps)
		set(EXPORT_NAME ${export_name})
		set(EXPORT_DEPS "${deps}")
		if (NOT IS_ABSOLUTE "${export_dest}")
			file(TO_CMAKE_PATH "${export_dest}" EXPORT_ROOT)
			string(REGEX REPLACE "(/)+[^/]+" "/.." EXPORT_ROOT "${EXPORT_ROOT}")
			string(REGEX REPLACE "^[^/]+" ".." EXPORT_ROOT "${EXPORT_ROOT}")
		endif ()
		# configure file on export script, etc.
		configure_file("${_int_install_exports_template}" "${CMAKE_CURRENT_BINARY_DIR}/${config_file}" @ONLY)
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${config_file}" DESTINATION "${export_dest}")
	endif ()
endfunction()
