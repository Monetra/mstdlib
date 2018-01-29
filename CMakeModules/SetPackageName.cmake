# SetPackageName.cmake
#
# Sets CPACK_PACKAGE_FILE_NAME to a more descriptive package name than the default one provided by CPack.
# Examples:
#    Mstdlib-Windows32-1.0.0
#    mstdlib-linux64-1.0.0
#    mstdlib-linux64-arm-1.0.0
#    Mstdlib-MacOS-1.0.0
#
# If the "WIN32_ABI" optional parameter is given, a C runtime library ABI description is added to the
# name, like "msvc19" when built with VS 2015/2017, or "mingw" when built with MinGW.
#
# set_package_name(product_name product_version [WIN32_ABI])

function(set_package_name name version)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(arch 64)
	else ()
		set(arch 32)
	endif ()

	set(force_lower FALSE)
	if (WIN32)
		set(sysname "Windows${arch}")
	elseif (IOS)
		set(sysname "iOS")
	elseif (APPLE)
		set(sysname "MacOS")
	elseif (ANDROID)
		set(sysname "Android")
	else ()
		set(sysname "${CMAKE_SYSTEM_NAME}${arch}")
		set(force_lower TRUE)
	endif ()

	set(CPACK_PACKAGE_FILE_NAME "${name}-${sysname}")

	if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
		string(APPEND CPACK_PACKAGE_FILE_NAME "-arm")
		# Note: 64-bit ARM is always hard-float. But for 32-bit, need to inform user that it was built with hard-float
		#       by adding 'hf'.
		if (arch EQUAL 32 AND (CMAKE_C_FLAGS MATCHES "-mfloat-abi=hardfp" OR CMAKE_CXX_FLAGS MATCHES "-mfloat-abi=hardfp"))
			string(APPEND CPACK_PACKAGE_FILE_NAME "hf")
		endif ()
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

	if (force_lower)
		string(TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME)
	endif ()

	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}" PARENT_SCOPE)
endfunction()

