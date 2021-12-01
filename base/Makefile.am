lib_LTLIBRARIES = libmstdlib.la

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)

libmstdlib_la_LDFLAGS = -export-dynamic -version-info @LIBTOOL_VERSION@
libmstdlib_la_SOURCES =                \
	\
	bincodec/m_base64.c                \
	bincodec/m_bincodec.c              \
	bincodec/m_bincodec_conv.c         \
	bincodec/m_hex.c                   \
	\
	cache/m_cache.c                    \
	cache/m_cache_strvp.c              \
	\
	data/m_bin.c                       \
	data/m_bit_buf.c                   \
	data/m_bit_parser.c                \
	data/m_bitlist.c                   \
	data/m_buf.c                       \
	data/m_chr.c                       \
	data/m_getopt.c                    \
	data/m_getopt_parse.c              \
	data/m_parser.c                    \
	data/m_queue.c                     \
	data/m_state_machine.c             \
	data/m_str.c                       \
	data/m_str_fmt.c                   \
	data/m_str_hexdump.c               \
	data/m_str_num.c                   \
	data/m_str_patmatch.c              \
	\
	fs/m_fs.c                          \
	fs/m_fs_dir.c                      \
	fs/m_fs_dir_entries.c              \
	fs/m_fs_dir_entry.c                \
	fs/m_fs_dir_walk.c                 \
	fs/m_fs_file.c                     \
	fs/m_fs_info.c                     \
	fs/m_fs_path.c                     \
	fs/m_fs_path_norm.c                \
	fs/m_fs_perms.c                    \
	fs/m_fs_progress.c                 \
	\
	hash/m_hash_dict.c                 \
	hash/m_hash_func.c                 \
	hash/m_hash_multi.c                \
	hash/m_hash_strbin.c               \
	hash/m_hash_stridx.c               \
	hash/m_hash_stru64.c               \
	hash/m_hash_strvp.c                \
	hash/m_hashtable.c                 \
	hash/m_hash_u64bin.c               \
	hash/m_hash_u64str.c               \
	hash/m_hash_u64u64.c               \
	hash/m_hash_u64vp.c                \
	\
	list/m_list.c                      \
	list/m_list_bin.c                  \
	list/m_list_str.c                  \
	list/m_list_u64.c                  \
	list/m_llist.c                     \
	list/m_llist_bin.c                 \
	list/m_llist_str.c                 \
	list/m_llist_u64.c                 \
	\
	math/m_bithacks.c                  \
	math/m_decimal.c                   \
	math/m_float.c                     \
	math/m_operator.c                  \
	math/m_prop.c                      \
	math/m_rand.c                      \
	math/m_round.c                     \
	\
	mem/m_endian.c                     \
	mem/m_mem.c                        \
	\
	sort/m_sort_binary.c               \
	sort/m_sort_compar.c               \
	sort/m_sort_mergesort.c            \
	sort/m_sort_qsort.c                \
	\
	time/m_time.c                      \
	time/m_time_elapsed.c              \
	time/m_time_gm.c                   \
	time/m_time_local.c                \
	time/m_time_parse.c                \
	time/m_time_tz_dst_rules.c         \
	time/m_time_tz_olson.c             \
	time/m_time_tz_olson_transitions.c \
	time/m_time_tz_posix.c             \
	time/m_time_tz_rule.c              \
	time/m_time_tzs.c                  \
	utf8/m_utf8.c                      \
	utf8/m_utf8_case.c                 \
	utf8/m_utf8_check.c                \
	utf8/m_utf8_tables.c

if WIN32
libmstdlib_la_SOURCES += \
		fs/m_fs_win.c                  \
		fs/m_fs_file_win.c             \
		fs/m_fs_info_win.c             \
		fs/m_fs_perms_win.c            \
		\
		platform/m_platform_win.c      \
		\
		time/m_time_tz_win.c
else
libmstdlib_la_SOURCES += \
		fs/m_fs_unx.c                  \
		fs/m_fs_file_unx.c             \
		fs/m_fs_info_unx.c             \
		fs/m_fs_perms_unx.c            \
		\
		platform/m_platform_unx.c
endif
