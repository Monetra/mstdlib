# Requires full target file path to be passed in as TARGET_FILE variable.
#
# e.g., cmake -DTARGET_FILE=<path to file> -P make_md5_file.cmake

cmake_minimum_required(VERSION 3.4.3)

if (NOT EXISTS "${TARGET_FILE}")
	message(FATAL_ERROR "md5 checksum failed: given file ${TARGET_FILE} does not exist!")
endif ()

execute_process(COMMAND ${CMAKE_COMMAND} -E md5sum ${TARGET_FILE}
	RESULT_VARIABLE res
	OUTPUT_VARIABLE md5sum
	ERROR_VARIABLE  err
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (NOT res EQUAL 0)
	message(FATAL_ERROR "md5 checksum creation failed: ${err}")
endif ()

file(WRITE "${TARGET_FILE}.md5" "${md5sum}\n")

