if (ANDROID)
	# Android defaults to only searching the path prefixes, but OpenSSL may be outside
	# the path prefixes
	set(_ORIG_OPENSSL_LIBRARY_MODE CMAKE_${FIND_ROOT_PATH_MODE_LIBRARY})
	set(_ORIG_OPENSSL_INCLUDE_MODE CMAKE_${FIND_ROOT_PATH_MODE_INCLUDE})
	set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
	set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
endif()

# Include the OpenSSL find module that's bundled with CMake.
if ((NOT OPENSSL_ROOT_DIR) AND (CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME))
	# If the user hasn't explicitly chosen a root dir, and we're not cross-compiling, try some hard-coded paths.
	if (UNIX)
		set(_ext 32)
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(_ext 64)
		endif ()
		if (EXISTS "/usr/local/ssl${_ext}")
			set(_guess "/usr/local/ssl${_ext}")
		else ()
			set(_guess "/usr/local/ssl")
		endif ()
	elseif (WIN32)
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(_guess "$ENV{ProgramFiles}/OpenSSL64")
		else ()
			set(_guess "$ENV{ProgramFiles}/OpenSSL")
		endif ()
	endif ()

	if (EXISTS "${_guess}")
		set(OPENSSL_ROOT_DIR "${_guess}")
	endif ()
endif ()
include("${CMAKE_ROOT}/Modules/FindOpenSSL.cmake")

# Create import targets for OpenSSL libraries (OpenSSL::Crypto and OpenSSL::SSL), if version of
# CMake FindOpenSSL module was too old to provide them (they were added in CMake 3.4).
if (NOT TARGET OpenSSL::Crypto AND EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
	add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
	set_target_properties(OpenSSL::Crypto PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES     "${OPENSSL_INCLUDE_DIR}"
		IMPORTED_LINK_INTERFACE_LANGUAGES "C"
		IMPORTED_LOCATION                 "${OPENSSL_CRYPTO_LIBRARY}"
	)
endif()
if (NOT TARGET OpenSSL::SSL AND EXISTS "${OPENSSL_SSL_LIBRARY}")
	add_library(OpenSSL::SSL UNKNOWN IMPORTED)
	set_target_properties(OpenSSL::SSL PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES     "${OPENSSL_INCLUDE_DIR}"
		IMPORTED_LINK_INTERFACE_LANGUAGES "C"
		IMPORTED_LOCATION                 "${OPENSSL_SSL_LIBRARY}"
	)
endif()

IF (ANDROID)
	SET (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ${_ORIG_OPENSSL_LIBRARY_MODE})
	SET (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${_ORIG_OPENSSL_INCLUDE_MODE})
ENDIF ()
