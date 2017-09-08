# Run this script with "cmake -P setup_thirdparty.cmake" to download dependencies that go in the thirdparty directory.
#
# Pass "-DGEN=<generator name>" to explicitly pick a generator
# Pass "-DCMAKE_BUILD_TYPE=<build type>" to explicitly pick a build type (defaults to RelWithDebInfo)
# Note that -D options must go BEFORE the -P.
#
cmake_minimum_required(VERSION 3.4.3)

set(rootdir ${CMAKE_CURRENT_LIST_DIR})

if (NOT EXISTS "${rootdir}/thirdparty")
	message(FATAL_ERROR "This script must be run from the root source dir of mstdlib!")
endif ()

find_program(GIT git)
if (NOT GIT)
	message(FATAL_ERROR "This script requires git to be installed.")
endif ()

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


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# LibCheck
file(REMOVE_RECURSE "${rootdir}/thirdparty/check")
file(REMOVE_RECURSE "${rootdir}/thirdparty/check_src")
execute_process(COMMAND "${GIT}" clone https://github.com/libcheck/check.git --depth 1 thirdparty/check_src
	WORKING_DIRECTORY "${rootdir}"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to download libcheck from server.")
endif ()
# libcheck hardcodes build type, comment this out so we can set it ourselves.
file(READ "${rootdir}/thirdparty/check_src/CMakeLists.txt" str)
string(REPLACE "set\(CMAKE_BUILD_TYPE" "\#set\(CMAKE_BUILD_TYPE" str "${str}")
file(WRITE "${rootdir}/thirdparty/check_src/CMakeLists.txt" "${str}")

# libcheck can't be chain-built, because the target names it uses internally conflict with
# existing targets in mstdlib. So, let's build it separately and install to the thirdparty dir.
file(MAKE_DIRECTORY "${rootdir}/thirdparty/check_src/build")
execute_process(COMMAND "${CMAKE_COMMAND}" "${rootdir}/thirdparty/check_src"
	${GEN}
	-DCHECK_ENABLE_TESTS=FALSE
	-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
	-DCMAKE_INSTALL_PREFIX=${rootdir}/thirdparty/check
	WORKING_DIRECTORY "${rootdir}/thirdparty/check_src/build"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to configure libcheck.")
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" --build . --target install --config ${CMAKE_BUILD_TYPE}
	WORKING_DIRECTORY "${rootdir}/thirdparty/check_src/build"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to build libcheck.")
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# C-Ares
file(REMOVE_RECURSE "${rootdir}/thirdparty/c-ares")
execute_process(COMMAND "${GIT}" clone https://github.com/c-ares/c-ares.git --depth 1 thirdparty/c-ares
	WORKING_DIRECTORY "${rootdir}"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to download c-ares from server.")
endif ()


# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# SQLite Amalgamation
file(REMOVE_RECURSE "${rootdir}/thirdparty/sqlite-amalgamation")
set(sqlite_name sqlite-amalgamation-3200100)
message("Downloading and extracting ${sqlite_name}.zip into thirdparty/sqlite-amalgamation ...")
file(DOWNLOAD https://sqlite.org/2017/${sqlite_name}.zip
	"${rootdir}/thirdparty/${sqlite_name}.zip"
	INACTIVITY_TIMEOUT 3
	TIMEOUT 20
	EXPECTED_HASH SHA256=38fb09f523857f4265248e3aaf4744263757288094033ccf2184594ec656e255
	STATUS res
)
if (NOT res MATCHES "^0;")
	list(GET res 1 err)
	message(FATAL_ERROR "Couldn't download sqlite amalgamation: ${err}")
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf ${sqlite_name}.zip --format=zip
	WORKING_DIRECTORY "${rootdir}/thirdparty"
	RESULT_VARIABLE res
)
if (NOT res EQUAL 0)
	message(FATAL_ERROR "Failed to extract sqlite amalgamation from zip file.")
endif ()
file(RENAME "${rootdir}/thirdparty/${sqlite_name}" "${rootdir}/thirdparty/sqlite-amalgamation")

