# Run this script with "cmake -P setup_thirdparty.cmake" to download dependencies that go in the thirdparty directory.
#
# Options for building libcheck:
#   Pass "-DBUILDCHECK=FALSE" to disable building of check.
#   Pass "-DBUILDDIR=<build dir>" to pick location to build libcheck in (defaults to build/, relative to current working directory)
#   Pass "-DTOOLCHAIN=<toolchain>" to cross-compile
#   Pass "-DGEN=<generator name>" to explicitly pick a generator
#   Pass "-DCMAKE_BUILD_TYPE=<build type>" to explicitly pick a build type (defaults to RelWithDebInfo)
# 
# (Note that -D options must go BEFORE the -P flag)
#
cmake_minimum_required(VERSION 3.4.3)

set(srcdir "${CMAKE_CURRENT_LIST_DIR}")

# BUILDCHECK is true by default
if (NOT DEFINED BUILDCHECK)
	set(BUILDCHECK TRUE)
endif ()

find_program(GIT git)
if (NOT GIT)
	message(FATAL_ERROR "This script requires git to be installed.")
endif ()

# Options for building code.
if (BUILDCHECK)
	if (BUILDDIR)
		get_filename_component(bindir "${BUILDDIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
	else ()
		set(bindir "${CMAKE_SOURCE_DIR}/build")
	endif ()

	file(MAKE_DIRECTORY "${bindir}")

	# If user didn't specify a generator, try to use ninja if available.
	if (NOT GEN)
		find_program(NINJA ninja)
		if (NINJA)
			set(GEN "Ninja")
		endif ()
	endif ()
	# If user didn't specify a generator and ninja wasn't available, use NMake if it's present. Otherwise, use platform default.
	if (NOT GEN)
		find_program(NMAKE nmake)
		if (NMAKE)
			set(GEN "NMake Makefiles")
		endif ()
	endif ()
	if (GEN)
		set(GEN "-G${GEN}")
	endif ()

	# If user didn't specify a build type, set default.
	if (NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE "RelWithDebInfo")
	endif ()
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# LibCheck
if (BUILDCHECK)
	file(REMOVE_RECURSE "${bindir}/thirdparty/check")
	file(REMOVE_RECURSE "${bindir}/thirdparty/check_src")
	execute_process(COMMAND "${GIT}" clone --branch 0.12.0 https://github.com/libcheck/check.git --depth 1 "${bindir}/thirdparty/check_src"
		RESULT_VARIABLE res
	)
	if (NOT res EQUAL 0)
		message(FATAL_ERROR "Failed to download libcheck from server.")
	endif ()
	# libcheck hardcodes build type, comment this out so we can set it ourselves.
	file(READ "${bindir}/thirdparty/check_src/CMakeLists.txt" str)
	string(REPLACE "set\(CMAKE_BUILD_TYPE" "\#set\(CMAKE_BUILD_TYPE" str "${str}")
	file(WRITE "${bindir}/thirdparty/check_src/CMakeLists.txt" "${str}")

	if (TOOLCHAIN)
		get_filename_component(abspath "${TOOLCHAIN}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
		set(toolchain_cmd "-DCMAKE_TOOLCHAIN_FILE=${abspath}")
	endif ()
	
	# libcheck can't be chain-built, because the target names it uses internally conflict with
	# existing targets in mstdlib. So, let's build it separately and install to the thirdparty dir.
	file(MAKE_DIRECTORY "${bindir}/thirdparty/check_src/build")
	execute_process(COMMAND "${CMAKE_COMMAND}" "${bindir}/thirdparty/check_src"
		${GEN}
		${toolchain_cmd}
		-DCHECK_ENABLE_TESTS=FALSE
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${bindir}/thirdparty/check
		WORKING_DIRECTORY "${bindir}/thirdparty/check_src/build"
		RESULT_VARIABLE res
	)
	if (NOT res EQUAL 0)
		message(FATAL_ERROR "Failed to configure libcheck.")
	endif ()

	execute_process(COMMAND "${CMAKE_COMMAND}" --build . --target install --config ${CMAKE_BUILD_TYPE}
		WORKING_DIRECTORY "${bindir}/thirdparty/check_src/build"
		RESULT_VARIABLE res
	)
	if (NOT res EQUAL 0)
		message(FATAL_ERROR "Failed to build libcheck.")
	endif ()
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# C-Ares
file(REMOVE_RECURSE "${srcdir}/thirdparty/c-ares")
execute_process(COMMAND "${GIT}" clone https://github.com/c-ares/c-ares.git --depth 1 "${srcdir}/thirdparty/c-ares"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to download c-ares from server.")
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# SQLite Amalgamation
file(REMOVE_RECURSE "${srcdir}/thirdparty/sqlite-amalgamation")
set(sqlite_name sqlite-amalgamation-3360000)
message("Downloading and extracting ${sqlite_name}.zip into ${src_dir}/thirdparty/sqlite-amalgamation ...")
file(DOWNLOAD https://sqlite.org/2021/${sqlite_name}.zip
	"${srcdir}/thirdparty/${sqlite_name}.zip"
	INACTIVITY_TIMEOUT 5
	TIMEOUT 40
	EXPECTED_HASH SHA1=a0eba79e5d1627946aead47e100a8a6f9f6fafff
	STATUS res
)
if (NOT res MATCHES "^0;")
	list(GET res 1 err)
	message(FATAL_ERROR "Couldn't download sqlite amalgamation: ${err}")
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf ${sqlite_name}.zip --format=zip
	WORKING_DIRECTORY "${srcdir}/thirdparty"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to extract sqlite amalgamation from zip file.")
endif ()
file(RENAME "${srcdir}/thirdparty/${sqlite_name}" "${srcdir}/thirdparty/sqlite-amalgamation")

