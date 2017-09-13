# InstallDepLibs.cmake
#
# Helper function for installing library dependencies.
#
# Accepts a list of paths and/or import targets. Import targets that are static libs or executables
# are silently ignored.
#
# Functions:
# install_deplibs([lib dest] [runtime dest] [... lib files or import targets ...])
#     Install the given list of lib dependencies to CMAKE_INSTALL_PREFIX, alongside the project. Can be passed
#     either library paths or imported targets. On Windows, if given an import lib path or an imported library of
#     type UNKNOWN, it will try to guess the path to the DLL. If it can't find the the DLL, it will throw an error.
#
#     By default, install_deplibs will also copy any DLL's to the build's runtime output directory ([build dir]/bin, usually).
#     This behavior can be turned off by setting INSTALL_DEPLIBS_COPY_DLL to FALSE (see below).
#
# copy_deplibs([... lib files or import targets])
#     Copy the given dependencies to the build directory, without installing them. Uses the same library resolution rules
#     as install_deplibs, the only difference is that it doesn't install anything.
#
# Extra variables that modify how the function call works:
# INSTALL_DEPLIBS_COPY_DLL  - if TRUE (the default), any DLL's installed with install_deplibs will also be copied to
#                             the bin dir of the build directory. Set to FALSE to disable DLL copies.
#
#                             NOTE: this option only affects install_deplibs(). copy_deplibs() always copies to build dir,
#                                   regardless of how this variable is set.
#
# INSTALL_DEPLIBS_COPY_DEST - directory to copy DLL's to. If not specified, defaults to CMAKE_RUNTIME_OUTPUT_DIR.
#


# Helper function for _install_deplibs_internal: try to find .dll using path of an import lib.
function(get_dll_from_implib out_dll path)
	# Get directory containing import lib, and try to guess root dir of install.
	get_filename_component(imp_dir "${path}" DIRECTORY)
	string(REGEX REPLACE "/[/0-9x_-]*lib[/0-9x_-]*(/.*|$)" "" root_dir "${imp_dir}")

	# Get library name by removing .lib from extension.
	get_filename_component(imp_file "${path}" NAME)
	string(REGEX REPLACE "\.lib$" "" libname "${imp_file}")
	string(MAKE_C_IDENTIFIER "${libname}" clibname)

	# Get alternate library names by removing lib prefix, and or d, MT, MDd, etc. (suffixes indicating visual studio build flags).
	string(REGEX REPLACE "^lib" "" nolibname "${libname}")

	string(REGEX REPLACE "M[dDtT]+$" "" alt_name "${libname}")
	list(APPEND alt_names ${alt_name})
	string(REGEX REPLACE "M[dDtT]+$" "" alt_name "${nolibname}")
	list(APPEND alt_names ${alt_name})
	string(REGEX REPLACE "[dD]$" "" alt_name "${libname}")
	list(APPEND alt_names ${alt_name})
	string(REGEX REPLACE "[dD]$" "" alt_name "${nolibname}")
	list(APPEND alt_names ${alt_name})

	# Figure out possible arch names, based on bitness of project.
	set(suffixes)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		list(APPEND suffixes
			64 bin64 64/bin bin/64 lib64 64/lib lib/64
			x86_64 x86_64/bin bin/x86_64 x86_64/lib lib/x86_64
		)
	else ()
		list(APPEND suffixes
			32 bin32 32/bin bin/32 lib32 32/lib lib/32
			x86 x86/bin bin/x86 x86/lib lib/x86
		)
	endif ()
	list(APPEND suffixes bin lib)

	set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
	find_library(${clibname}_DLL
		NAMES           ${libname} ${nolibname} ${alt_names}
		HINTS           "${imp_dir}"
		                "${root_dir}"
		NO_DEFAULT_PATH
		PATH_SUFFIXES   ${suffixes}
	)

	if (${clibname}_DLL)
		set(${out_dll} "${${clibname}_DLL}" PARENT_SCOPE)
	else ()
		message(FATAL_ERROR "install_dep_libs() couldn't find DLL for given import lib \"${path}\" (set path with -D${clibname}_DLL=...)")
	endif ()
endfunction()


# Helper function for _install_deplibs_internal: convert given list of libs into paths.
function(get_paths_from_libs lib_dest runtime_dest out_paths_name out_libs_name)
	set(out_libs)
	foreach (lib IN LISTS ${out_libs_name})
		# Skip "optimized" and "debug" keywords that might be in a <NAME>_LIBRARIES variable.
		if (lib STREQUAL "optimized" OR lib STREQUAL "debug")
			continue()
		endif ()

		if (TARGET "${lib}")
			# If this is an alias target, get the proper name of the target, then add the result back
			# onto the list of libs to process on the next invocation of this function.
			get_target_property(alias ${lib} ALIASED_TARGET)
			if (alias)
				list(APPEND out_libs "${alias}")
				continue()
			endif ()

			# If this target isn't a shared, module or interface library target, skip it silently without doing anything.
			get_target_property(type ${lib} TYPE)
			if (NOT type STREQUAL "SHARED_LIBRARY" AND
				NOT type STREQUAL "MODULE_LIBRARY" AND
				NOT type STREQUAL "INTERFACE_LIBRARY" AND
				NOT type STREQUAL "UNKNOWN_LIBRARY")  #UNKNOWN is a special type that only applies to import libraries.
				continue()
			endif ()

			# If this target isn't imported, install directly if shared, then skip regardless of type.
			get_target_property(is_imported ${lib} IMPORTED)
			if (NOT is_imported)
				if (type STREQUAL "SHARED_LIBRARY")
					install(TARGETS ${lib}
						LIBRARY DESTINATION "${lib_dest}"
						RUNTIME DESTINATION "${runtime_dest}"
					)
				endif ()
				continue()
			endif ()

			# For imported interface libraries, grab the value of interface link libraries, and add the results
			# back onto the list of libs to process on the next invocation of this function.
			if (type STREQUAL "INTERFACE_LIBRARY")
				get_target_property(dep_libs ${lib} INTERFACE_LINK_LIBRARIES)
				if (dep_libs)
					list(APPEND out_libs "${dep_libs}")
				endif ()
				continue()
			endif ()

			# For shared/module imported libs, get the imported location (should be DLL, on Windows). Add to list of paths.
			get_target_property(lib_path ${lib} LOCATION)
			if (WIN32 AND NOT lib_path)
				# If there's no known DLL, use the import lib as the path instead.
				get_target_property(lib_path ${lib} IMPORTED_IMPLIB)
			endif ()
			if (NOT EXISTS "${lib_path}")
				message(FATAL_ERROR "Target ${lib} given to install_dep_libs() contained bad path ${lib_path}")
			endif ()
			list(APPEND ${out_paths_name} "${lib_path}")
		elseif (lib)
			# Handling for if this lib is a file path.
			if (NOT EXISTS "${lib}")
				message(FATAL_ERROR "Path ${lib} given to install_dep_libs() was bad")
			endif ()
			list(APPEND ${out_paths_name} "${lib}")
		endif ()
	endforeach ()

	set(${out_paths_name} "${${out_paths_name}}" PARENT_SCOPE)
	set(${out_libs_name} "${out_libs}" PARENT_SCOPE)
endfunction()


# Helper function for _install_deplibs_internal: retrieve soname of given lib file. If no soname, returns the path.
find_program(READELF readelf DOC "readelf (unix/ELF only)")
function(read_soname outvarname path)
	# Set output variable to empty string - this is what will be returned on an error.
	set(${outvarname} "" PARENT_SCOPE)

	if (NOT READELF) # If this system doesn't provide the readelf command.
		return()
	endif ()

	# Read the ELF header from the file.
	execute_process(COMMAND "${READELF}" -d ${path}
		RESULT_VARIABLE res
		OUTPUT_VARIABLE header
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if (NOT res EQUAL 0) # If the readelf command returned an error status code.
		return()
	endif ()

	# Parse the SONAME out of the header.
	if (NOT header MATCHES "\\(SONAME\\)[^\n]+\\[([^\n]+)\\]") # If output didn't contain SONAME field.
		return()
	endif ()

	set(${outvarname} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()


# Helper function for install_deplibs and copy_deplibs.
# _install_deplibs_internal([lib dest] [runtime dest] [flag to turn file copy on/off] [flag to turn file install on/off] [... lib files or import targets ...]
function(_install_deplibs_internal lib_dest runtime_dest do_copy do_install)
	if ((NOT do_copy) AND (NOT do_install))
		return()
	endif ()

	# Handle default destination for copied DLL's.
	if (NOT INSTALL_DEPLIBS_COPY_DEST)
		set(INSTALL_DEPLIBS_COPY_DEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	endif ()

	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	# Construct list of library paths.
	set(lib_paths)
	if (NOT lib_dest)
		set(lib_dest lib)
	endif ()
	if (NOT runtime_dest)
		set(runtime_dest bin)
	endif ()

	set(libs ${ARGN})
	while (libs)
		get_paths_from_libs("${lib_dest}" "${runtime_dest}" lib_paths libs)
	endwhile ()

	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	# Process each library path, install to appropriate location.
	set(sonames)
	foreach (path IN LISTS lib_paths)
		# AIX apparently links against the .so filename, not the one with versioning info.
		set(aix_libname)
		if (OSTYPE STREQUAL "aix")
			get_filename_component(aix_libname "${path}" NAME)
		endif ()

		# If on Windows, try to replace import libraries with DLL's. Throws fatal error if it can't do it.
		if (WIN32 AND ${path} MATCHES "\.lib$")
			get_dll_from_implib(path "${path}")
		endif ()

		# Resolve any symlinks in path to get actual physical name. If relative, evaluate relative to current binary dir.
		get_filename_component(path "${path}" REALPATH BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

		# If we're on AIX, we need our own special install handling. Run it, then skip to next iteration.
		if (aix_libname AND do_install)
			install(FILES "${path}" RENAME "${aix_libname}" DESTINATION "${lib_dest}")
			continue()
		endif ()

		# Figure out the destination (DLL's and manifest files go to runtime_dest, everything else goes to lib_dest).
		if (path MATCHES "\.[dD][lL][lL]$" OR path MATCHES "\.[mM][aA][nN][iI][fF][eE][sS][tT]$")
			set(dest "${runtime_dest}")
			if (path MATCHES "\.[dD][lL][lL]$")
				set(type PROGRAMS) # install DLL's as executable
			else ()
				set(type FILES) # install manifests as a normal non-executable file
			endif ()

			# If requested by caller, copy the DLL's to the build dir in addition to installing them.
			# If the file with the same name and timestamp already exists at the destination, nothing will be copied.
			if (do_copy)
				file(COPY "${path}" DESTINATION "${INSTALL_DEPLIBS_COPY_DEST}")
			endif ()
		else ()
			set(dest "${lib_dest}")
			set(type FILES)
		endif ()

		if (NOT do_install)
			continue()
		endif ()

		# Install the file.
		#message(STATUS "will install ${path} to ${dest}")
		install(${type} "${path}" DESTINATION "${dest}")

		# If the library has a soname that's different than the actual name of the file on disk, add a symlink.
		read_soname(soname "${path}")
		if (soname)
			get_filename_component(libname "${path}" NAME)
			if (NOT soname STREQUAL libname)
				# Generate a relative-path symlink in the build dir (doesn't have to be valid).
				set(tmpdir "${CMAKE_CURRENT_BINARY_DIR}/dep-lib-links")
				file(MAKE_DIRECTORY "${tmpdir}")
				file(REMOVE "${tmpdir}/${soname}") # Remove any old symlink with the same name.
				execute_process(
					COMMAND           ${CMAKE_COMMAND} -E create_symlink "${libname}" "${soname}"
					WORKING_DIRECTORY "${tmpdir}"
					RESULT_VARIABLE   res
					ERROR_QUIET
					OUTPUT_QUIET
				)

				if (NOT res EQUAL 0)
					message(AUTHOR_WARNING "install_deplib: failed to create install symlink for \"${libname}\"")
					continue()
				endif ()

				# Install the symlink to the same directory as the library.
				#message(STATUS "will install ${tmpdir}/${soname} to ${dest}")
				install(${type} "${tmpdir}/${soname}" DESTINATION "${dest}")
			endif ()
		endif ()
	endforeach ()
endfunction()


# install_deplibs([lib dest] [runtime dest] [... lib files or import targets ...]
function(install_deplibs lib_dest runtime_dest)
	# Handle default values for variables that control DLL copying.
	if (NOT DEFINED INSTALL_DEPLIBS_COPY_DLL)
		set(INSTALL_DEPLIBS_COPY_DLL TRUE)
	endif ()

	# Call internal helper
	_install_deplibs_internal("${lib_dest}" "${runtime_dest}" ${INSTALL_DEPLIBS_COPY_DLL} TRUE ${ARGN})
endfunction()


# copy_deplibs([... lib files or import targets...])
function(copy_deplibs)
	_install_deplibs_internal("" "" TRUE FALSE ${ARGN})
endfunction()

