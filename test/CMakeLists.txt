include(InstallDepLibs) #provides copy_deplibs() and install_deplibs()

enable_testing()

if (NOT MSTDLIB_BUILD_TESTS)
	message(STATUS "Building tests... Disabled")
	return()
endif ()

# A few unit tests produce false failures on really slow/busy machines. Since they aren't reliable,
# need a way to disable them when we're doing the build as part of a continuous-integration suite.
option(MSTDLIB_BUILD_FOR_CI "Are we building tests to be run by a continuous-integration suite?" FALSE)
mark_as_advanced(FORCE MSTDLIB_BUILD_FOR_CI)

# Find libcheck (required).
set(old_lib_mode ${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY})
set(old_inc_mode ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE})
if (NOT Check_DIR AND EXISTS "${Mstdlib_BINARY_DIR}/thirdparty/check")
	# If user didn't specify a particular check installation, look in thirdparty dir.
	set(Check_DIR "${Mstdlib_BINARY_DIR}/thirdparty/check")
	# If we're cross-compiling, temporarily allow libs to be found outside the system root.
	if (NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
		set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
		set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
	endif ()
endif ()

find_package(Check)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ${old_lib_mode})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${old_inc_mode})

if (NOT TARGET Check::check)
	message(STATUS "Building tests... Disabled (requested, but can't be built because the Check library wasn't found)")
	return ()
endif ()

message(STATUS "Building tests... Enabled")

# Find optional dependencies.
find_package(Valgrind)

if (TARGET Valgrind::valgrind AND MSTDLIB_USE_VALGRIND)
	if (M_ASAN)
		message(STATUS "Valgrind requested for tests, but can't be used while ASAN is enabled. Run \"cmake -DM_ASAN=FALSE .\" or \"cmake -DMSTDLIB_USE_VALGRIND=FALSE .\"")
		set(MSTDLIB_USE_VALGRIND FALSE)
	else ()
		message(STATUS "Running tests with Valgrind... Enabled")
	endif ()
else ()
	message(STATUS "Running tests with Valgrind... Disabled")
endif ()

set(tests
	# base
	base/check_types.c
	base/bincodec/check_bincodec.c
	base/cache/check_cache_strvp.c
	base/data/check_bit_buf.c
	base/data/check_bit_parser.c
	base/data/check_buf.c
	base/data/check_chr.c
	base/data/check_getopt.c
	base/data/check_parser.c
	base/data/check_snprintf.c
	base/data/check_state_machine.c
	base/data/check_str.c
	base/data/check_str_int64.c
	base/data/check_str_uint64.c
	base/data/check_utf8.c
	base/fs/check_file.c
	base/fs/check_path.c
	base/hash/check_hash_dict.c
	base/hash/check_hash_multi.c
	base/hash/check_hash_strvp.c
	base/hash/check_hash_u64str.c
	base/list/check_list_u64.c
	base/list/check_llist_u64.c
	base/math/check_decimal.c
	base/math/check_rand.c
	base/math/check_round.c
	base/mem/check_mem.c
	base/time/check_time_fmt.c
	base/time/check_time_tm.c
	base/time/check_time_tz.c
)
set(slow_tests)
#formats
if (MSTDLIB_BUILD_FORMATS)
	list(APPEND tests
		formats/check_conf.c
		formats/check_csv.c
		formats/check_email.c
		formats/check_email_reader.c
		formats/check_ini.c
		formats/check_json.c
		formats/check_http_reader.c
		formats/check_http_simple_reader.c
		formats/check_http_simple_writer.c
		formats/check_http2.c
		formats/check_http2_reader.c
		formats/check_mtzfile.c
		formats/check_settings.c
		formats/check_table.c
		formats/check_url.c
		formats/check_xml.c
		formats/check_xml_entities.c
	)
endif ()
# thread
if (MSTDLIB_BUILD_THREAD)
	if (NOT M_ASAN AND NOT MSTDLIB_BUILD_FOR_CI)
		# The coop test yields false-positives when built with ASAN.
		list(APPEND slow_tests
			thread/check_thread_coop.c
		)
	endif ()
	list(APPEND slow_tests
		thread/check_thread_native.c
	)
endif ()
# io
if (MSTDLIB_BUILD_IO)
	list(APPEND tests
		io/check_event_loopback.c
		io/check_event_net.c
		io/check_block_net.c
		io/check_event_pipe.c
		io/check_dns.c
		io/check_serial.c
		io/check_pipespeed.c
		io/check_process.c
		io/check_netspeed.c
		io/check_event_bwshaping.c
	)
	list(APPEND slow_tests
		io/check_event_interactions.c
		io/check_event_timer.c
	)
	if (${CMAKE_SYSTEM_NAME} MATCHES "Windows"
		OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux"
		OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		list(APPEND tests
			io/check_hid.c
		)
	endif()
endif()
# tls
if(MSTDLIB_BUILD_TLS)
	list(APPEND tests
		tls/check_tls.c
		tls/check_block_tls.c
		tls/check_tlsspeed.c
	)
endif()
# sql
if(MSTDLIB_BUILD_SQL)
	list(APPEND tests
		sql/check_sql.c
	)
endif()
# sql
if(MSTDLIB_BUILD_TEXT)
	list(APPEND tests
		text/check_puny.c
		text/check_re.c
		text/check_codec_utf8.c
		text/check_verify.c
	)
endif()
# net
if(MSTDLIB_BUILD_NET)
	list(APPEND tests
		net/check_smtp.c
	)
endif()

# Include the slow tests in the list of tests we're going to run, if not explicitly disabled by the user.
if (NOT MSTDLIB_SKIP_SLOW_TESTS)
	list(APPEND tests ${slow_tests})
endif ()

# list of link dependencies for each test.
set(test_deps Check::check Mstdlib::base Mstdlib::config)

if (TARGET Mstdlib::formats)
	list(APPEND test_deps Mstdlib::formats)
endif ()
if (TARGET Mstdlib::thread)
	list(APPEND test_deps Mstdlib::thread)
endif ()
if (TARGET Mstdlib::io)
	list(APPEND test_deps Mstdlib::io)
endif ()
if (TARGET Mstdlib::tls)
	list(APPEND test_deps Mstdlib::tls)
endif ()
if (TARGET Mstdlib::sql)
	list(APPEND test_deps Mstdlib::sql)
endif ()
if (TARGET Mstdlib::text)
	list(APPEND test_deps Mstdlib::text)
endif ()
if (TARGET Mstdlib::net)
	list(APPEND test_deps Mstdlib::net)
endif()
if (UNIX AND NOT APPLE)
	list(APPEND test_deps m)
endif ()

# There is a bug in check where it needs to link to threads. try and force it to happen.
# IF we include threads then that gets linked via mstdlib thread otherwise we need to add it.
if (MSTDLIB_BUILD_THREAD)
	find_package(Threads)
	if (CMAKE_HAVE_PTHREAD_H)
		list(APPEND test_deps pthread)
	endif ()
endif ()

# Add an executable target for each test, then add the target to the list of CPack tests.
foreach (test_src ${tests})
	get_filename_component(test_prog ${test_src} NAME_WE)

	add_executable(${test_prog} ${test_src})
	target_link_libraries(${test_prog} PRIVATE ${test_deps})

	if (MSTDLIB_BUILD_FOR_CI)
		# Disable iffy tests, if we're building on Travis, AppVeyor, etc.
		target_compile_definitions(${test_prog} PRIVATE MSTDLIB_BUILD_FOR_CI)
	endif ()

	if (MSVC)
		# Some unit tests in here have embedded UTF-8 characters. Need to specify an extra flag to
		# get VS to accept the file.
		target_compile_options(${test_prog} PRIVATE -utf-8)
	endif ()

	if (WIN32 AND test_prog STREQUAL "check_sql")
		target_compile_definitions(${test_prog} PRIVATE _CRT_SECURE_NO_DEPRECATE) #because check_sql uses getenv(...)
	endif ()

	if (TARGET Valgrind::valgrind AND MSTDLIB_USE_VALGRIND)
		add_test(
			NAME    ${test_prog}_memcheck
			COMMAND Valgrind::valgrind "--leak-check=full"
									   "--show-reachable=yes"
									   "--num-callers=24"
									   "--suppressions=${PROJECT_SOURCE_DIR}/valgrind/check.suppressions"
									   $<TARGET_FILE:${test_prog}> "from_ctest"
		)
	else ()
		add_test(
			NAME    ${test_prog}
			COMMAND ${test_prog} "from_ctest"
		)
	endif ()
endforeach ()

# Need helper program for smtp proceess tests
if(MSTDLIB_BUILD_NET)
	add_executable(sendmail_emu net/sendmail_emu.c)
	target_link_libraries(sendmail_emu PRIVATE ${test_deps})
endif()

# Make Check target
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
