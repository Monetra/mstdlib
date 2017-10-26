# SetPackageName.cmake
#
# Sets CPACK_PACKAGE_FILE_NAME to a more descriptive package name than the default one provided by CPack.
# Examples:
#    mstdlib-windows32-1.0.0
#    mstdlib-linux64-1.0.0
#    mstdlib-linux64-arm-1.0.0
#
# If the "WIN32_ABI" optional parameter is given, a C runtime library ABI description is added to the
# name, like "msvc19" when built with VS 2015/2017, or "mingw" when built with MinGW.
#
# set_package_name(product_name product_version [WIN32_ABI])

function(set_package_name name version)
	set(CPACK_PACKAGE_FILE_NAME ${name}-${CMAKE_SYSTEM_NAME})
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
		string(APPEND CPACK_PACKAGE_FILE_NAME "-arm")
	endif ()
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		string(APPEND CPACK_PACKAGE_FILE_NAME "64")
	else ()
		string(APPEND CPACK_PACKAGE_FILE_NAME "32")
	endif ()

	if ("WIN32_ABI" IN_LIST ARGN)
		if (WIN32 AND MINGW)
			string(APPEND CPACK_PACKAGE_FILE_NAME "-mingw")
		elseif (WIN32)
			# Visual Studio - grab first two digits of the compiler version (19 == VS 2015 or 2017).
			# Note that the first two digits denote compatibility for the C and C++ runtimes. For example,
			# an executable built with version 19.xx will be linked againsta a different set of C/C++ runtimes
			# than one built with version 18.xx or version 20.xx.
			string(SUBSTRING "${MSVC_VERSION}" 0 2 ver)
			string(APPEND CPACK_PACKAGE_FILE_NAME "-msvc${ver}")
		endif ()
	endif ()

	string(APPEND CPACK_PACKAGE_FILE_NAME "-${version}")
	string(TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME)

	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}" PARENT_SCOPE)
endfunction()
