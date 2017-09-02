# InstallExports.cmake
#
# Helper function for installing export config files ([name]Config.cmake, etc).
#
# install_exports([export name] [export installation destination])
#
# Ex: install_exports(Monetra share/Monetra/cmake)

include(CMakePackageConfigHelpers)

function(install_exports export_name export_dest)
	# Generate *Version.cmake file and install it.
	set(ver_file "${CMAKE_CURRENT_BINARY_DIR}/${export_name}Version.cmake")

	include(CMakePackageConfigHelpers)
	write_basic_package_version_file("${ver_file}" COMPATIBILITY SameMajorVersion)

	install(FILES "${ver_file}" DESTINATION "${export_dest}")

	# Install *Config.cmake and *Config-[build type].cmake files.
	install(EXPORT ${export_name}
		NAMESPACE   ${export_name}::
		DESTINATION "${export_dest}"
		FILE        ${export_name}Config.cmake
	)
endfunction()
