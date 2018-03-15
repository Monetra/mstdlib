# InstallExports.cmake
#
# Helper function for installing export config files ([name]Config.cmake, etc).
#
# Adding dependency names (OpenSSL, ZLIB, etc.) to end of arg list will create exported
# files that call the FindDependencyMacro.
#
# install_exports([export name] [export installation destination] [... variable number of dependencies ...])
#
# Ex: install_exports(Monetra share/Monetra/cmake)

include(CMakePackageConfigHelpers)

# Path to template file - have to set it outside the function, because it's relative to this file,
# not the file that's calling the install_exports() function.
set(_int_install_exports_template "${CMAKE_CURRENT_LIST_DIR}/InstallExportsScript.cmake.in")

function(install_exports export_name export_dest)
	# Generate *Version.cmake file and install it.
	set(ver_file "${CMAKE_CURRENT_BINARY_DIR}/${export_name}Version.cmake")

	include(CMakePackageConfigHelpers)
	write_basic_package_version_file("${ver_file}" COMPATIBILITY SameMajorVersion)

	install(FILES "${ver_file}" DESTINATION "${export_dest}")

	set(config_file ${export_name}Config.cmake)
	if (ARGN)
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

	if (ARGN)
		set(EXPORT_NAME ${export_name})
		set(EXPORT_DEPS "${ARGN}")
		# configure file on export script, etc.
		configure_file("${_int_install_exports_template}" "${CMAKE_CURRENT_BINARY_DIR}/${config_file}" @ONLY)
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${config_file}" DESTINATION "${export_dest}")
	endif ()
endfunction()
