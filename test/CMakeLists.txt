enable_testing()

if (MSTDLIB_BUILD_TESTS)
	find_package(Check)
	find_package(Valgrind)
endif ()

if (TARGET Check::check AND MSTDLIB_BUILD_TESTS)
	message(STATUS "Building tests... Enabled")

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
			formats/check_csv.c
			formats/check_ini.c
			formats/check_json.c
			formats/check_mtzfile.c
			formats/check_settings.c
			formats/check_verify.c
			formats/check_xml.c
			formats/check_xml_entities.c
		)
	endif ()
	# thread
	if (MSTDLIB_BUILD_THREAD)
		list(APPEND slow_tests
			thread/check_thread_coop.c
			thread/check_thread_native.c
		)
	endif ()
	# io
	if (MSTDLIB_BUILD_IO)
		list(APPEND tests
			io/check_event_loopback.c
			io/check_event_net.c
			io/check_event_timer.c
			io/check_event_pipe.c
			io/check_dns.c
			io/check_serial.c
			io/check_pipespeed.c
			io/check_netspeed.c
		)
		list(APPEND slow_tests
			io/check_block_net.c
			io/check_event_bwshaping.c
		)
		if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
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

	# tls
	if(MSTDLIB_BUILD_SQL)
		list(APPEND tests
			sql/check_sql.c
		)
	endif()

	# Include the slow tests in the list of tests we're going to run, if not explicitly disabled by the user.
	if (NOT MSTDLIB_SKIP_SLOW_TESTS)
		list(APPEND tests ${slow_tests})
	endif ()

	# There is a bug in check where it needs to link to threads. try and force it to happen.
	# IF we include threads then that gets linked via mstdlib thread otherwise we need to add it.
	set(check_threads)
	if (MSTDLIB_BUILD_THREAD)
		find_package(Threads)
		if (CMAKE_HAVE_PTHREAD_H)
			set(check_threads "pthread")
		endif ()
	endif ()
	
	# list of link dependencies for each test.
	set(test_deps Check::check ${check_threads} Mstdlib::base Mstdlib::config)
	
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
	if (UNIX AND NOT APPLE)
		list(APPEND test_deps m)
	endif ()
	
	# Add an executable target for each test, then add the target to the list of CPack tests.
	foreach (test_src ${tests})
		get_filename_component(test_prog ${test_src} NAME_WE)
		
		add_executable(${test_prog} ${test_src})
		target_link_libraries(${test_prog} PRIVATE ${test_deps})
		
		if (TARGET Valgrind::valgrind AND MSTDLIB_USE_VALGRIND)
			add_test(
				NAME    ${test_prog}_memcheck
				COMMAND Valgrind::valgrind "--leak-check=full"
				                           "--show-reachable=yes"
				                           "--suppressions=${PROJECT_SOURCE_DIR}/valgrind/check.suppressions"
				                           "--log-file=${PROJECT_BINARY_DIR}/test/${test_prog}-valgrind.log"
				                           $<TARGET_FILE:${test_prog}>
			)
		else ()
			add_test(
				NAME    ${test_prog}
				COMMAND ${test_prog}
			)
		endif ()
	endforeach ()

	# Make Check target
	add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
else ()
	message(STATUS "Building tests... Disabled")
endif ()
