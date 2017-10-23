cmake_minimum_required(VERSION 3.4.3)

# Most of the modules will take the form of mstdlib-NAME but
# for base we just want mstdlib.
project(mstdlib
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

set(sources
	# bincodec:
	bincodec/m_base64.c
	bincodec/m_bincodec.c
	bincodec/m_bincodec_conv.c
	bincodec/m_hex.c

	# cache:
	cache/m_cache.c
	cache/m_cache_strvp.c

	# data:
	data/m_bin.c
	data/m_bit_buf.c
	data/m_bit_parser.c
	data/m_buf.c
	data/m_chr.c
	data/m_getopt.c
	data/m_getopt_parse.c
	data/m_parser.c
	data/m_queue.c
	data/m_state_machine.c
	data/m_str.c
	data/m_str_fmt.c
	data/m_str_hexdump.c
	data/m_str_num.c
	data/m_str_patmatch.c

	# fs:
	fs/m_fs.c
	fs/m_fs_dir.c
	fs/m_fs_dir_entries.c
	fs/m_fs_dir_entry.c
	fs/m_fs_dir_walk.c
	fs/m_fs_file.c
	fs/m_fs_info.c
	fs/m_fs_path.c
	fs/m_fs_path_norm.c
	fs/m_fs_perms.c
	fs/m_fs_progress.c

	# hash:
	hash/m_hash_dict.c
	hash/m_hash_func.c
	hash/m_hash_multi.c
	hash/m_hash_strbin.c
	hash/m_hash_stridx.c
	hash/m_hash_strvp.c
	hash/m_hashtable.c
	hash/m_hash_u64bin.c
	hash/m_hash_u64str.c
	hash/m_hash_u64u64.c
	hash/m_hash_u64vp.c

	# list:
	list/m_list_bin.c
	list/m_list.c
	list/m_list_str.c
	list/m_list_u64.c
	list/m_llist_bin.c
	list/m_llist.c
	list/m_llist_str.c
	list/m_llist_u64.c

	# math:
	math/m_bithacks.c
	math/m_decimal.c
	math/m_float.c
	math/m_operator.c
	math/m_prop.c
	math/m_rand.c
	math/m_round.c

	# mem:
	mem/m_endian.c
	mem/m_mem.c

	# sort:
	sort/m_sort_binary.c
	sort/m_sort_compar.c
	sort/m_sort_mergesort.c
	sort/m_sort_qsort.c

	# time:
	time/m_time.c
	time/m_time_elapsed.c
	time/m_time_gm.c
	time/m_time_local.c
	time/m_time_parse.c
	time/m_time_tz_dst_rules.c
	time/m_time_tz_olson.c
	time/m_time_tz_olson_transitions.c
	time/m_time_tz_posix.c
	time/m_time_tz_rule.c
	time/m_time_tzs.c
)

if (WIN32 OR MINGW)
	list(APPEND sources
		# fs:
		fs/m_fs_win.c
		fs/m_fs_file_win.c
		fs/m_fs_info_win.c
		fs/m_fs_perms_win.c
		# platform:
		platform/m_platform_win.c
		# time:
		time/m_time_tz_win.c
	)
else ()
	list(APPEND sources
		# fs:
		fs/m_fs_unx.c
		fs/m_fs_file_unx.c
		fs/m_fs_info_unx.c
		fs/m_fs_perms_unx.c
		# platform:
		platform/m_platform_unx.c
	)
endif ()


# Build the library.
if (MSTDLIB_STATIC)
	add_library(${PROJECT_NAME} STATIC
		${sources}
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
	if (${MSTDLIB_STATIC_PIC})
		set_target_properties(${PROJECT_NAME} PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)
	endif ()
else ()
	add_library(${PROJECT_NAME} SHARED
		${sources}
	)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		POSITION_INDEPENDENT_CODE TRUE
	)
endif ()

# Set include directories.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)


# Link against any dependencies from other modules.
target_link_libraries(${PROJECT_NAME}
	PRIVATE Mstdlib::config
)


# Link against any extra system libraries.
if (NEED_RT)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE rt
	)
endif ()
if (WIN32 OR MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE winmm.lib Advapi32.lib Authz.lib Shlwapi.lib Shell32.lib
	)
endif ()


# Versioning on the library.
if (NOT ANDROID)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		VERSION   ${PROJECT_VERSION}
		SOVERSION ${PROJECT_SOVERSION}
		EXPORT_NAME base
	)
endif ()

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()


# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::base ALIAS ${PROJECT_NAME})
