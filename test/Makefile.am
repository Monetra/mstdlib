if HAVE_CHECK

AM_CFLAGS   = @CHECK_CFLAGS@ -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/base -DMSTDLIB_INTERNAL=1
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = @CHECK_LIBS@
AM_CFLAGS  += -DTEST
AM_LDFLAGS += -L$(top_builddir)/base/.libs/
LDADD       = $(top_builddir)/base/libmstdlib.la

######################################################################

TESTS_ENVIRONMENT = $(VALGRIND_ENVIRONMENT)

######################################################################

TESTS = \
	base/check_types \
	base/bincodec/check_bincodec \
	base/data/check_bit_buf \
	base/data/check_bit_parser \
	base/data/check_buf \
	base/data/check_chr \
	base/data/check_getopt \
	base/data/check_parser \
	base/data/check_snprintf \
	base/data/check_state_machine \
	base/data/check_str \
	base/data/check_str_int64 \
	base/data/check_str_uint64 \
	base/fs/check_file \
	base/fs/check_path \
	base/hash/check_hash_dict \
	base/hash/check_hash_multi \
	base/hash/check_hash_strvp \
	base/hash/check_hash_u64str \
	base/list/check_list_u64 \
	base/list/check_llist_u64 \
	base/math/check_decimal \
	base/math/check_rand \
	base/math/check_round \
	base/mem/check_mem \
	base/time/check_time_fmt \
	base/time/check_time_tm \
	base/time/check_time_tz


if MSTDLIB_FORMATS
TESTS +=  \
	formats/check_csv \
	formats/check_ini \
	formats/check_json \
	formats/check_mtzfile \
	formats/check_settings \
	formats/check_verify \
	formats/check_xml \
	formats/check_xml_entities
AM_LDFLAGS += -L$(top_builddir)/formats/.libs/
LDADD += $(top_builddir)/formats/libmstdlib_formats.la
endif

if MSTDLIB_THREAD
TESTS +=  \
		thread/check_thread_coop \
		thread/check_thread_native
AM_LDFLAGS += -L$(top_builddir)/thread/.libs/
LDADD += $(top_builddir)/thread/libmstdlib_thread.la
endif

if MSTDLIB_IO
TESTS += \
		io/check_event_loopback \
		io/check_event_net \
		io/check_block_net \
		io/check_event_timer \
		io/check_event_pipe \
		io/check_dns \
		io/check_event_bwshaping \
		io/check_serial \
		io/check_pipespeed \
		io/check_netspeed

# No HID support on MacOSX
if !MACOSX
TESTS += io/check_hid
endif

AM_LDFLAGS += -L$(top_builddir)/io/.libs/
LDADD += $(top_builddir)/io/libmstdlib_io.la
endif

if MSTDLIB_TLS
TESTS += \
		tls/check_tls \
		tls/check_tlsspeed 
AM_LDFLAGS += -L$(top_builddir)/tls/.libs/
LDADD += $(top_builddir)/tls/libmstdlib_tls.la -lssl -lcrypto
endif

if MSTDLIB_SQL
TESTS += \
		sql/check_sql 
AM_LDFLAGS += -L$(top_builddir)/sql/.libs/
LDADD += $(top_builddir)/sql/libmstdlib_sql.la
endif

######################################################################

all = check

check_PROGRAMS = $(TESTS)

endif
