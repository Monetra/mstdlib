m4_define([mstdlib_major_version],   [1])
m4_define([mstdlib_minor_version],   [0])
m4_define([mstdlib_release_version], [0])
m4_define([mstdlib_version], [mstdlib_major_version.mstdlib_minor_version.mstdlib_release_version])

AC_PREREQ([2.60])
AC_INIT([mstdlib], [mstdlib_version], [support@monetra.com])

dnl helper programs
AC_CONFIG_AUX_DIR([config])
dnl macros
AC_CONFIG_MACRO_DIR([m4])
dnl default install point
dnl AC_PREFIX_DEFAULT([/usr/local])
dnl headers
AC_CONFIG_HEADERS([config.h])
dnl generated files
AC_CONFIG_FILES([Makefile include/Makefile base/Makefile formats/Makefile thread/Makefile io/Makefile tls/Makefile log/Makefile sql/Makefile sql/sqlite/Makefile sql/mysql/Makefile sql/postgresql/Makefile sql/oracle/Makefile sql/odbc/Makefile test/Makefile doc/Makefile Doxyfile])

AC_USE_SYSTEM_EXTENSIONS
dnl AC_PROG_CC([clang llvm-gcc gcc])
AC_LANG([C])
AC_PROG_CC_STDC
AM_PROG_CC_C_O
dnl __typeof__ -> typeof
AC_C_TYPEOF
AM_PROG_AS

dnl Setting this prevents showing the full compile/link command line.
dnl Use --disable-silent-rules  or  make V=1  to re-enable verbosity.
AM_SILENT_RULES([yes])

ADD_OBJECTS=""
UNAME_S=`uname -s`
UNAME_V=`uname -v`

dnl include code coverage
m4_include([m4/ax_prog_doxygen.m4])
m4_include([m4/ax_append_flag.m4])
m4_include([m4/ax_check_compile_flag.m4])
m4_include([m4/ax_append_compile_flags.m4])
DX_INIT_DOXYGEN([mstdlib], Doxyfile, doc/api)
DX_DOXYGEN_FEATURE(ON)
DX_DOT_FEATURE(ON)
DX_HTML_FEATURE(ON)

AX_CHECK_LINK_FLAG([-Wl,-rpath-link,/usr], [have_rpath_link="yes"], [have_rpath_link="no"])

dnl required to build
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl ------- LIBCHECK ---------
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(CHECK, [check >= 0.9.4],,[AC_MSG_RESULT([not found with pkgconfig])])
check_dirs="/usr /usr/local /usr/local/check"
AC_ARG_WITH(check-dir, 
	AC_HELP_STRING([--with-check-dir=<dir>], [Paths to search for libcheck (default=$check_dirs)]),
	[ check_dirs=$withval ],
	[ ])

dnl use manual way since pkgconfig didn't work
if test "x$CHECK_LIBS" = "x" ; then
	AC_MSG_CHECKING(for libcheck)
	for dir in $check_dirs ; do
		exts="so sl dylib a"
		for ext in $exts ; do
			if test -r "$dir/lib/libcheck.${ext}" ; then
				CHECK_HEADER_DIR="$dir/include"
				CHECK_LIB_DIR="$dir/lib"
				break
			elif test -r "$dir/lib/x86_64-linux-gnu/libcheck.${ext}" ; then
				CHECK_HEADER_DIR="$dir/include"
				CHECK_LIB_DIR="$dir/lib/x86_64-linux-gnu"
				break
			elif test -r "$dir/lib/x86-linux-gnu/libcheck.${ext}" ; then
				CHECK_HEADER_DIR="$dir/include"
				CHECK_LIB_DIR="$dir/lib/x86-linux-gnu"
				break
			elif test -r "$dir/lib64/libcheck.${ext}" ; then
				CHECK_HEADER_DIR="$dir/include"
				CHECK_LIB_DIR="$dir/lib64"
				break
			fi
		done
		if test "x$CHECK_LIB_DIR" != "x" ; then
			break
		fi
	done
	if test "x$CHECK_LIB_DIR" = "x" ; then
		AC_MSG_RESULT(libcheck not found)
	else
		CHECK_CFLAGS="-I${CHECK_HEADER_DIR}"
		CHECK_LIBS="-L${CHECK_LIB_DIR} -lcheck"
		AC_MSG_RESULT($CHECK_LIB_DIR)
	fi
fi

AM_CONDITIONAL(HAVE_CHECK, [test "x$CHECK_LIBS" != "x"])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)
dnl --------------------------

dnl fairly severe build strictness
dnl change foreign to gnu or gnits to comply with gnu standards
AM_INIT_AUTOMAKE([foreign subdir-objects 1.9.6])
AC_DISABLE_STATIC
AC_ENABLE_SHARED

AC_ARG_ENABLE(internal-build,
	AC_HELP_STRING([--enable-internal-build], [Disables checks for precompiled dependencies and uses different search paths]),
	[ INTERNAL_BUILD=${enableval} ],
	[ INTERNAL_BUILD=no ])


AC_ARG_ENABLE(valgrind-testing,
	AC_HELP_STRING([--enable-valgrind-testing], [enable running of tests inside Valgrind (disabled by default)]),
	[ if test "$enable_valgrind_testing" = "yes"; then
		VALGRIND_DIR="$PWD/valgrind"
		AC_CHECK_PROG(CHECK_VALGRIND, valgrind, yes, no)
		VALGRIND_ENVIRONMENT="$VALGRIND_ENVIRONMENT valgrind -q"
		VALGRIND_ENVIRONMENT+=" --memcheck:leak-check=full"
		VALGRIND_ENVIRONMENT+=" --show-reachable=yes"
		VALGRIND_ENVIRONMENT+=" --num-callers=100"
		VALGRIND_ENVIRONMENT+=" --track-fds=no"
		VALGRIND_ENVIRONMENT+=" --error-exitcode=1"
		VALGRIND_ENVIRONMENT+=" --suppressions=$VALGRIND_DIR/check.suppressions"
		AC_SUBST(VALGRIND_ENVIRONMENT)
	fi
	]
)

AC_CANONICAL_HOST()

case $host_os in
	*cygwin*|*mingw* )
		os_type=windows
		;;
	*linux*)
		os_type=linux
		;;
	*darwin*|*macosx*)
		os_type=macosx
		;;
	*)
		os_type=posix
		;;
esac

AM_CONDITIONAL([WIN32], [ test $os_type = windows ])
AM_CONDITIONAL([LINUX], [ test $os_type = linux ])
AM_CONDITIONAL([MACOSX], [ test $os_type = macosx ])

BUILD_SUBDIRS="include base doc"

dnl
dnl Checks for programs, libraries, headers, typedefs, structures, compiler
dnl  characteristics
dnl

dnl functions
AC_CHECK_FUNCS([strcasecmp strncasecmp])
AC_CHECK_FUNCS([localtime_r])
AC_CHECK_FUNCS([getpwuid_r getpwnam_r getgrgid_r getgrnam_r])
AC_CHECK_FUNCS([readdir_r])
AC_CHECK_FUNCS([secure_getenv])
AC_CHECK_FUNCS([inet_pton inet_ntop sigtimedwait sigwait])

dnl header files
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdalign.h sys/time.h time.h io.h errno.h unistd.h])
AC_CHECK_HEADERS([sys/types.h sys/regset.h])
AC_CHECK_HEADERS([valgrind/valgrind.h])
AC_CHECK_HEADERS([sys/ioctl.h sys/select.h sys/socket.h sys/un.h poll.h signal.h])
AC_CHECK_HEADERS([netinet/in.h netinet/tcp.h netdb.h arpa/inet.h])
dnl libs
AC_CHECK_LIB(rt, clock_gettime, [], [])

AC_CHECK_TYPE(socklen_t, [ AC_DEFINE(HAVE_SOCKLEN_T, [], [socklen_t]) ], [ ] , [
             #include <sys/types.h>
             #include <sys/socket.h>
 ] )

AC_CHECK_TYPE([struct sockaddr_storage], [ AC_DEFINE(HAVE_SOCKADDR_STORAGE, [], [struct sockaddr_storage]) ], [ ] , [
             #include <sys/socket.h>
             #include <netinet/in.h>
 ] )
 
dnl structures
AC_MSG_CHECKING(to see if struct dirent has d_type)
AC_TRY_COMPILE([
                 #include <dirent.h>
                ], [
                 struct dirent mydir;
                 mydir.d_type = DT_UNKNOWN;
                ], has_dirent_type="yes", has_dirent_type="no")
AC_MSG_RESULT($has_dirent_type)
if test "x$has_dirent_type" = "xyes" ; then
     AC_DEFINE([HAVE_DIRENT_TYPE], [], [struct dirent has d_type])
fi

AC_MSG_CHECKING(to see if struct tm has tm_gmtoff)
AC_TRY_COMPILE([
                 #include <stdio.h>
                 #include <time.h>
                ], [
                 struct tm mytm;
                 (void)mytm.tm_gmtoff;
               ], struct_tm_has_gmtoff="yes", struct_tm_has_gmtoff="no")
AC_MSG_RESULT($struct_tm_has_gmtoff)
if test "x$struct_tm_has_gmtoff" = "xyes" ; then
	AC_DEFINE([STRUCT_TM_HAS_GMTOFF], [], [struct tm has tm_gmtoff])
fi

AC_MSG_CHECKING(to see if struct tm has tm_zone)
AC_TRY_COMPILE([
                 #include <stdio.h>
                 #include <time.h>
                ], [
                 struct tm mytm;
                 (void)mytm.tm_zone;
               ], struct_tm_has_zone="yes", struct_tm_has_zone="no")
AC_MSG_RESULT($struct_tm_has_zone)
if test "x$struct_tm_has_zone" = "xyes" ; then
	AC_DEFINE([STRUCT_TM_HAS_ZONE], [], [struct tm has tm_zone])
fi

dnl va_copy
AC_MSG_CHECKING(to see if has va_copy)
AC_TRY_LINK([
		        #include <stdarg.h>
			],
			[
			    va_list a,b;
			    va_copy(a,b);
			 ], has_va_copy="yes", has_va_copy="no")
AC_MSG_RESULT($has_va_copy)
if test "x$has_va_copy" = "xyes" ; then
     AC_DEFINE([HAVE_VA_COPY], [], [has va copy])
fi

AC_MSG_CHECKING(to see if has __va_copy)
AC_TRY_LINK([
		        #include <stdarg.h>
			],
			[
			    va_list a,b;
			    __va_copy(a,b);
            ], has__va_copy="yes", has__va_copy="no")
AC_MSG_RESULT($has__va_copy)
if test "x$has__va_copy" = "xyes" ; then
     AC_DEFINE([HAVE___VA_COPY], [], [has  va copy])
fi

AC_MSG_CHECKING(to see if has va_list is array type)
AC_TRY_LINK([
		        #include <stdarg.h>
			],
			[
			    va_list a,b;
			    a = b;
            ], va_list_is_array="yes", va_list_is_array="no")
AC_MSG_RESULT($va_list_is_array)
if test "x$va_list_is_array" = "xyes" ; then
     AC_DEFINE([VA_LIST_IS_ARRAY_TYPE], [], [va list is array])
fi


if test "x$va_list_is_array" = "xno" ; then
	AC_MSG_CHECKING(to see if has va_list is pointer type)
AC_TRY_LINK([
		        #include <stdarg.h>
			],
			[
			    va_list a,b;
			    *a = *b;
		    ], va_list_is_pointer="yes", va_list_is_pointer="no")
	AC_MSG_RESULT($va_list_is_pointer)
	if test "x$va_list_is_pointer" = "xyes" ; then
		 AC_DEFINE([VA_LIST_IS_POINTER_TYPE], [], [va list is pointer])
	fi
fi



dnl ----- Check for Position Independent Executable flags -----
AC_MSG_CHECKING([whether $CC accepts PIE flags])
save_CFLAGS=$CFLAGS
save_LDFLAGS=$LDFLAGS
CFLAGS="$CFLAGS -fPIE"
LDFLAGS="$LDFLAGS -pie"
AC_TRY_RUN([static int foo[30000]; int main () { return 0; }],
           [pie_supported=yes], [pie_supported=no], [pie_supported=no])
AC_MSG_RESULT($pie_supported)
if test "$pie_supported" = "no" -o "$use_hardening" = "no" ; then
  CFLAGS=$save_CFLAGS
  LDFLAGS=$save_LDFLAGS
fi

dnl ----- Check for RELRO linker flags -----
AC_MSG_CHECKING([whether $LD accepts RELRO flags])
save_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,-z,relro -Wl,-z,now"
AC_TRY_RUN([static int foo[30000]; int main () { return 0; }],
           [relro_supported=yes], [relro_supported=no], [relro_supported=no])
AC_MSG_RESULT($relro_supported)
if test "$relro_supported" = "no" -o "$use_hardening" = "no"; then
  LDFLAGS=$save_LDFLAGS
fi

dnl ----- Check for noexecstack linker flags -----
AC_MSG_CHECKING([whether $LD accepts noexecstack flags])
save_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,-z,noexecstack"
AC_TRY_RUN([static int foo[30000]; int main () { return 0; }],
           [noexecstack_supported=yes], [noexecstack_supported=no], [noexecstac_supported=no])
AC_MSG_RESULT($noexecstack_supported)
if test "$noexecstack_supported" = "no" -o "$use_hardening" = "no"; then
  LDFLAGS=$save_LDFLAGS
fi


dnl ----- Check for visibility flags -----
AC_MSG_CHECKING([for visibility declarations])
save_CFLAGS="$CFLAGS"
CFLAGS="-fvisibility=hidden -Werror"
AC_TRY_COMPILE([extern __attribute__((__visibility__("hidden"))) int hiddenvar;
                extern __attribute__((__visibility__("default"))) int exportedvar;
                extern __attribute__((__visibility__("hidden"))) int hiddenfunc (void);
                extern __attribute__((__visibility__("default"))) int exportedfunc (void);],
               [],
               visibility_supported=yes,
               visibility_supported=no)
CFLAGS=$save_CFLAGS
AC_MSG_RESULT(${visibility_supported})
if test "$visibility_supported" = "yes" ; then
        CFLAGS="$CFLAGS -fvisibility=hidden"
fi

LT_INIT([win32-dll,shared,disable-static,pic,disable-fast-install,aix-soname=svr4])

dnl
dnl CFLAGS
dnl

AX_APPEND_COMPILE_FLAGS([\
	-W -Wall -Wextra -Wchar-subscripts -Wcomment -Wno-coverage-mismatch \
	-Wdouble-promotion -Wformat -Wnonnull -Winit-self \
	-Wignored-qualifiers -Wmain \
	-Wmissing-braces -Wmissing-include-dirs -Wparentheses -Wsequence-point \
	-Wreturn-type -Wswitch -Wtrigraphs -Wunused-but-set-parameter \
	-Wunused-but-set-variable -Wunused-function -Wunused-label \
	-Wunused-local-typedefs -Wunused-parameter -Wunused-variable -Wunused-value \
	-Wunused -Wuninitialized -Wmaybe-uninitialized -Wunknown-pragmas \
	-Wmissing-format-attribute -Warray-bounds \
	-Wtrampolines -Wfloat-equal \
	-Wdeclaration-after-statement -Wundef -Wshadow \
	-Wpointer-arith -Wtype-limits \
	-Wcast-align -Wwrite-strings -Wclobbered -Wempty-body \
	-Wenum-compare -Wjump-misses-init -Wsign-compare -Wsizeof-pointer-memaccess \
	-Waddress -Wlogical-op \
	-Wstrict-prototypes -Wold-style-declaration -Wold-style-definition \
	-Wmissing-parameter-type -Wmissing-prototypes -Wmissing-declarations \
	-Wmissing-field-initializers -Woverride-init -Wpacked -Wredundant-decls \
	-Wnested-externs -Winvalid-pch -Wvariadic-macros -Wvarargs \
	-Wvla -Wpointer-sign -Wdisabled-optimization -Wendif-labels -Wpacked-bitfield-compat \
	-Wformat-security -Wstrict-aliasing \
	-Wstrict-overflow -Wsync-nand -Wvolatile-register-var \
	-Wconversion -Wsign-conversion \
	-Werror=implicit-int \
	-Werror=implicit-function-declaration \
	-Werror=partial-availability \
], [CFLAGS], [-Werror])

case "${UNAME_S}" in
SCO_SV)
	;;
Darwin)
	AX_APPEND_COMPILE_FLAGS([-D_XOPEN_SOURCE=500 -D_DARWIN_C_SOURCE=1], [CPPFLAGS], [-Werror])
	;;
*)
	AX_APPEND_COMPILE_FLAGS([-std=c99], [CFLAGS], [-Werror])
	;;
esac

dnl -------------------- CHECKING FOR getgrgid_r() -----------------------

grgid="0"
dnl point to this is to first verify getgrgid_r exists, then
dnl in a multi-tiered fashion, check for which implementation exists.
AC_CHECK_FUNC(getgrgid_r, [
	AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
#       include <unistd.h>
	], [
		struct group   grp;
		struct group  *grp_result;
		char           pg_buf[16384];
		(void)getgrgid_r(getgid(), &grp, pg_buf, sizeof(pg_buf), &grp_result);
	], grgid="5",
	[
		AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
#       include <unistd.h>
		], [
			struct group   grp;
			char           pg_buf[16384];
			(void)getgrgid_r(getgid(), &grp, pg_buf, sizeof(pg_buf));
		], grgid="4"
		)
	]
	)
], grgid="0")

if test "$grgid" = "0" ; then
AC_CHECK_FUNC(getgrgid, [
                AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
#       include <unistd.h>
                ], [
					(void)getgrgid(getgid());
                ], grgid="1", grgid="0")
        ], grgid="1", [ grgid="0" ]
)
fi

if test "$grgid" = "5" ; then
  AC_DEFINE([HAVE_GETGRGID_5], 1, [getgrgid_r takes 5 args])
  AC_SUBST(HAVE_GETGRGID_5)
elif test "$grgid" = "4" ; then
  AC_DEFINE([HAVE_GETGRGID_4], 1, [getgrgid_r takes 3 args])
  AC_SUBST(HAVE_GETGRGID_4)
elif test "$grgid" = "1" ; then
  AC_DEFINE([HAVE_GETGRGID_1], 1, [getgrgid takes 1 arg])
  AC_SUBST(HAVE_GETGRGID_1)
elif test "$grgid" = "0" ; then
  AC_DEFINE([HAVE_GETGRGID_0], 1, [getgrgid does not exist])
  AC_SUBST(HAVE_GETGRGID_0)
fi

dnl -------------------- CHECKING FOR getgrnam_r() -----------------------

grnam="0"
dnl point to this is to first verify getgrnam_r exists, then
dnl in a multi-tiered fashion, check for which implementation exists.
AC_CHECK_FUNC(getgrnam_r, [
	AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
	], [
		struct group   grp;
		struct group  *grp_result;
		char           pg_buf[16384];
		(void)getgrnam_r("a", &grp, pg_buf, sizeof(pg_buf), &grp_result);
	], grnam="5",
	[
		AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
		], [
			struct group   grp;
			char           pg_buf[16384];
			(void)getgrnam_r("a", &grp, pg_buf, sizeof(pg_buf));
		], grnam="4"
		)
	]
	)
], grnam="0")

if test "$grnam" = "0" ; then
AC_CHECK_FUNC(getgrnam, [
                AC_TRY_COMPILE([
#       include <grp.h>
#       include <sys/types.h>
                ], [
					(void)getgrnam("a");
                ], grnam="1", grnam="0")
        ], grnam="1", [ grnam="0" ]
)
fi

if test "$grnam" = "5" ; then
  AC_DEFINE([HAVE_GETGRNAM_5], 1, [getgrnam_r takes 5 args])
  AC_SUBST(HAVE_GETGRNAM_5)
elif test "$grnam" = "4" ; then
  AC_DEFINE([HAVE_GETGRNAM_4], 1, [getgrnam_r takes 3 args])
  AC_SUBST(HAVE_GETGRNAM_4)
elif test "$grnam" = "1" ; then
  AC_DEFINE([HAVE_GETGRNAM_1], 1, [getgrnam takes 1 arg])
  AC_SUBST(HAVE_GETGRNAM_1)
elif test "$grnam" = "0" ; then
  AC_DEFINE([HAVE_GETGRNAM_0], 1, [getgrnam does not exist])
  AC_SUBST(HAVE_GETGRNAM_0)
fi

dnl -------------------- CHECKING FOR getpwuid_r() -----------------------

pwuid="0"
dnl point to this is to first verify getpwuid_r exists, then
dnl in a multi-tiered fashion, check for which implementation exists.
AC_CHECK_FUNC(getpwuid_r, [
	AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
	], [
					struct passwd *pwd_result;
					struct passwd  pwd;
					char           pbuf[16384];
					(void)getpwuid_r(getuid(), &pwd, pbuf, sizeof(pbuf), &pwd_result);
	], pwuid="5",
	[
		AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
		], [
					struct passwd  pwd;
					char           pbuf[16384];
					(void)getpwuid_r(getuid(), &pwd, pbuf, sizeof(pbuf));
		], pwuid="4"
		)
	]
	)
], pwuid="0")

if test "$pwuid" = "0" ; then
AC_CHECK_FUNC(getpwuid, [
                AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
                ], [
					(void)getpwuid(getuid());
                ], pwuid="1", pwuid="0")
        ], pwuid="1", [ pwuid="0" ]
)
fi

if test "$pwuid" = "5" ; then
  AC_DEFINE([HAVE_GETPWUID_5], 1, [getpwuid_r takes 5 args])
  AC_SUBST(HAVE_GETPWUID_5)
elif test "$pwuid" = "4" ; then
  AC_DEFINE([HAVE_GETPWUID_4], 1, [getpwuid_r takes 3 args])
  AC_SUBST(HAVE_GETPWUID_4)
elif test "$pwuid" = "1" ; then
  AC_DEFINE([HAVE_GETPWUID_1], 1, [getpwuid takes 1 arg])
  AC_SUBST(HAVE_GETPWUID_1)
elif test "$pwuid" = "0" ; then
  AC_DEFINE([HAVE_GETPWUID_0], 1, [getpwuid does not exist])
  AC_SUBST(HAVE_GETPWUID_0)
fi

dnl -------------------- CHECKING FOR getpwuid_r() -----------------------

pwnam="0"
dnl point to this is to first verify getpwnam_r exists, then
dnl in a multi-tiered fashion, check for which implementation exists.
AC_CHECK_FUNC(getpwnam_r, [
	AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
	], [
					struct passwd *pwd_result;
					struct passwd  pwd;
					char           pbuf[16384];
					(void)getpwnam_r("a", &pwd, pbuf, sizeof(pbuf), &pwd_result);
	], pwnam="5",
	[
		AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
		], [
					struct passwd  pwd;
					char           pbuf[16384];
					(void)getpwnam_r("a", &pwd, pbuf, sizeof(pbuf));
		], pwnam="4"
		)
	]
	)
], pwnam="0")

if test "$pwnam" = "0" ; then
AC_CHECK_FUNC(getpwnam, [
                AC_TRY_COMPILE([
#       include <pwd.h>
#       include <sys/types.h>
#       include <unistd.h>
                ], [
					(void)getpwnam("a");
                ], pwnam="1", pwnam="0")
        ], pwnam="1", [ pwnam="0" ]
)
fi

if test "$pwnam" = "5" ; then
  AC_DEFINE([HAVE_GETPWNAM_5], 1, [getpwnam_r takes 5 args])
  AC_SUBST(HAVE_GETPWNAM_5)
elif test "$pwnam" = "4" ; then
  AC_DEFINE([HAVE_GETPWNAM_4], 1, [getpwnam_r takes 3 args])
  AC_SUBST(HAVE_GETPWNAM_4)
elif test "$pwnam" = "1" ; then
  AC_DEFINE([HAVE_GETPWNAM_1], 1, [getpwnam takes 1 arg])
  AC_SUBST(HAVE_GETPWNAM_1)
elif test "$pwnam" = "0" ; then
  AC_DEFINE([HAVE_GETPWNAM_0], 1, [getpwnam does not exist])
  AC_SUBST(HAVE_GETPWNAM_0)
fi

dnl -------------------- CHECKING FOR readdir_r() -----------------------

readdir="0"
dnl point to this is to first verify getpwuid_r exists, then
dnl in a multi-tiered fashion, check for which implementation exists.
AC_CHECK_FUNC(getpwuid_r, [
	AC_TRY_COMPILE([
				#include <dirent.h>
				#include <stddef.h>
				#include <stdlib.h>
				#include <string.h>
				#include <sys/types.h>
	], [
					DIR           *dir;
					struct dirent *entry;
					struct dirent *result;
					size_t         len;
					len   = offsetof(struct dirent, d_name) + 16385;
					entry = malloc(len);
					memset(entry, 0, len);
					dir = opendir(".");
					(void)readdir_r(dir, entry, &result); 
					free(entry);
					closedir(dir);
	], readdir="3",
	[
		AC_TRY_COMPILE([
				#include <dirent.h>
				#include <stddef.h>
				#include <stdlib.h>
				#include <string.h>
				#include <sys/types.h>
		], [
					DIR           *dir;
					struct dirent *entry;
					size_t         len;
					len   = offsetof(struct dirent, d_name) + 16385;
					entry = malloc(len);
					memset(entry, 0, len);
					dir = opendir(".");
					(void)readdir_r(dir, entry); 
					free(entry);
					closedir(dir);
		], readdir="2"
		)
	]
	)
], readdir="0")

if test "$readdir" = "0" ; then
AC_CHECK_FUNC(getpwuid, [
                AC_TRY_COMPILE([
				#include <dirent.h>
				#include <sys/types.h>
                ], [
					DIR           *dir;
					dir = opendir(".");
					(void)readdir(dir);
					closedir(dir);
                ], readdir="1", readdir="0")
        ], readdir="1", [ readdir="0" ]
)
fi

if test "$readdir" = "3" ; then
  AC_DEFINE([HAVE_READDIR_3], 1, [readdir_r takes 3 args])
  AC_SUBST(HAVE_READDIR_3)
elif test "$readdir" = "2" ; then
  AC_DEFINE([HAVE_READDIR_2], 1, [readdir_r takes 2 args])
  AC_SUBST(HAVE_READDIR_2)
elif test "$readdir" = "1" ; then
  AC_DEFINE([HAVE_READDIR_1], 1, [readdir takes 1 arg])
  AC_SUBST(HAVE_READDIR_1)
elif test "$readdir" = "0" ; then
  AC_DEFINE([HAVE_READDIR_0], 1, [readdir does not exist])
  AC_SUBST(HAVE_READDIR_0)
fi


dnl
dnl formats
dnl
AC_ARG_ENABLE(mstdlib-formats,
	AC_HELP_STRING([--disable-mstdlib-formats], [Disable building of mstdlib formats library]),
	[ build_mstdlib_formats=${enableval} ],
	[ build_mstdlib_formats=yes ])

if test "${build_mstdlib_formats}" = "yes" ; then
	BUILD_SUBDIRS="${BUILD_SUBDIRS} formats"
fi


dnl
dnl Threading
dnl
AC_ARG_ENABLE(mstdlib-thread,
	AC_HELP_STRING([--disable-mstdlib-thread], [Disable building of thread library (and any dependent libraries such as io/tls)]),
	[ build_mstdlib_thread=${enableval} ],
	[ build_mstdlib_thread=yes ])

if test "${build_mstdlib_thread}" = "yes" ; then
	if test "${UNAME_S}" = "SCO_SV" -a "${UNAME_V}" = "5.0.6" ; then
		have_sco5="yes"
	else
		have_sco5="no"
	fi
	AC_CHECK_HEADER(pthread.h, [ have_pthreads="yes" ], [ have_pthreads="no" ])
	AC_CHECK_HEADERS([atomic.h])

	if test "$have_pthreads" = "yes" -a "$have_sco5" = "no" ; then
		AC_DEFINE([HAVE_PTHREAD], [], [Build with pthreads support])
		AC_SUBST(HAVE_PTHREAD)
		AC_DEFINE([HAVE_PTHREAD_H], [], [Have pthread.h])
		AC_SUBST(HAVE_PTHREAD_H)

		AC_CHECK_LIB(pthread, pthread_create, [ LIBPTHREAD="-lpthread" ], [ LIBPTHREAD="" ], [])
		AC_SUBST(LIBPTHREAD)

		case "${UNAME_S}" in
			FreeBSD|OpenBSD)
				CFLAGS="$CFLAGS -pthread"
				;;
			SCO_SV)
				CFLAGS="$CFLAGS -Kpthread"
				;;
		esac

		dnl --------- FIND pthread conditional functions/types ----------------
		AC_CHECK_DECL(pthread_init, [ pthread_init="yes"], [ pthread_init="no" ], [ #include <pthread.h> ])
		if test "$pthread_init" = "yes" ; then
			AC_DEFINE([HAVE_PTHREAD_INIT], [], [pthread_init exists])
			AC_SUBST(HAVE_PTHREAD_INIT)
		fi
		AC_SEARCH_LIBS(pthread_yield, pthread pthreads thread gthreads gthread c_r, [ pthread_yield="yes" ], [ pthread_yield="no" ])
		if test "$pthread_yield" = "yes" ; then
			AC_DEFINE([HAVE_PTHREAD_YIELD], [], [pthread_yield function])
			AC_SUBST(HAVE_PTHREAD_YIELD)
		fi
		AC_CHECK_DECL(pthread_rwlockattr_init, [ pthread_rwlockattr_init="yes"], [ pthread_rwlockattr_init="no" ], [ #include <pthread.h> ])
		if test "$pthread_rwlockattr_init" = "yes" ; then
			AC_DEFINE([HAVE_PTHREAD_RWLOCK_INIT], [], [pthread_rwlockattr_init exists])
			AC_SUBST(HAVE_PTHREAD_RWLOCK_INIT)
		fi
		AC_CHECK_DECL(pthread_rwlockattr_setkind_np, [ pthread_rwlockattr_setkind_np="yes"], [ pthread_rwlockattr_setkind_np="no" ], [ #include <pthread.h> ])
		if test "$pthread_rwlockattr_setkind_np" = "yes" ; then
			AC_DEFINE([HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP], [], [pthread_rwlockattr_setkind_np exists])
			AC_SUBST(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
		fi

		dnl -------------------- CHECKING TO SEE IF _REENTRANT IS VALID --------------------
		AC_MSG_CHECKING(to see if defining _REENTRANT works)
		AC_TRY_COMPILE([
		#               define _REENTRANT 1
		#               include <pthread.h>
		#               include <stdio.h>
		#               include <stdlib.h>
		#               include <unistd.h>
					], [
						pthread_t thread;
						printf("testing _REENTRANT\n");
					], is_reentrant="yes", is_reentrant="no")

		if test "$is_reentrant" = "yes" ; then
			AC_MSG_RESULT(yes)
			REENTRANT="-D_REENTRANT"
		else
			AC_MSG_RESULT(no)
			REENTRANT=""
		fi

		dnl Timing
		AC_CHECK_FUNC(poll, [ have_poll="yes" ], [ have_poll="no" ])
		AC_CHECK_FUNC(select, [ have_select="yes" ], [ have_select="no" ])
		AC_CHECK_FUNC(usleep, [ have_usleep="yes" ], [ have_usleep="no" ])

		if test "${UNAME_S}" = "freebsd" -o "${UNAME_S}" = "hpux"; then
		  AC_DEFINE([PTHREAD_SLEEP_USE_SELECT], [], [use select for sleeping])
		  AC_SUBST(PTHREAD_SLEEP_USE_SELECT)
		elif test "$have_poll" = "yes" ; then
		  AC_DEFINE([PTHREAD_SLEEP_USE_POLL], [], [use poll for sleeping])
		  AC_SUBST(PTHREAD_SLEEP_USE_POLL)
		elif test "$have_select" = "yes" ; then
		  AC_DEFINE([PTHREAD_SLEEP_USE_SELECT], [], [use select for sleeping])
		  AC_SUBST(PTHREAD_SLEEP_USE_SELECT)
		elif test "$have_usleep" = "yes" ; then
		  AC_DEFINE([PTHREAD_SLEEP_USE_NANOSLEEP], [], [use usleep for sleeping])
		  AC_SUBST(PTHREAD_SLEEP_USE_NANOSLEEP)
		fi
	fi
	AM_CONDITIONAL(HAVE_PTHREAD, test "$have_pthreads" = "yes" -a "$have_sco5" = "no")

	AC_CHECK_FUNC(getcontext, [ have_getcontext="yes" ], [ have_getcontext="no" ])
	if test "$have_getcontext" = "yes" ; then
		AC_DEFINE([HAVE_GETCONTEXT], [], [Have getcontext stack switching functions])
	fi

	AC_CHECK_FUNC(sysconf, [ have_sysconf="yes" ], [ have_sysconf="no" ])
	if test "$have_sysconf" = "yes" ; then
		AC_DEFINE([HAVE_SYSCONF], [], [Have sysconf function])
	fi

	BUILD_SUBDIRS="${BUILD_SUBDIRS} thread"
fi

dnl
dnl Io
dnl
AC_ARG_ENABLE(mstdlib-io,
	AC_HELP_STRING([--disable-mstdlib-io], [Disable building of io library (and any dependent libraries such as tls)]),
	[ build_mstdlib_io=${enableval} ],
	[ build_mstdlib_io=${build_mstdlib_thread} ])

if test "${build_mstdlib_io}" = "yes" ; then
	if test "${build_mstdlib_thread}" = "no" ; then
		AC_MSG_ERROR(IO REQUIRES THREAD)
	fi

	AC_CHECK_FUNC(kqueue, [ have_kqueue="yes" ], [ have_kqueue="no" ])
	if test "$have_kqueue" = "yes" ; then
		AC_DEFINE([HAVE_KQUEUE], [], [Use kqueue for file descriptor polling])
	fi
	AM_CONDITIONAL([HAVE_KQUEUE], [ test $have_kqueue = yes ])

	AC_CHECK_FUNC(epoll_create, [ have_epoll="yes" ], [ have_epoll="no"])
	if test "$have_epoll" = "yes" ; then
		AC_DEFINE([HAVE_EPOLL], [], [Use epoll for file descriptor polling])
	fi
	AM_CONDITIONAL([HAVE_EPOLL], [ test $have_epoll = yes ])

	AC_CHECK_FUNC(epoll_create1, [ have_epoll_create1="yes" ], [ have_epoll_create1="no"])
	if test "$have_epoll_create1" = "yes" ; then
		AC_DEFINE([HAVE_EPOLL_CREATE1], [], [Use epoll_create1 for CLOEXEC])
	fi

	AC_CHECK_FUNC(accept4, [ have_accept4="yes" ], [ have_accept4="no"])
	if test "$have_accept4" = "yes" ; then
		AC_DEFINE([HAVE_ACCEPT4], [], [Use accept4 for SOCK_CLOEXEC])
	fi

	AC_CHECK_FUNC(pipe2, [ have_pipe2="yes" ], [ have_pipe2="no"])
	if test "$have_pipe2" = "yes" ; then
		AC_DEFINE([HAVE_PIPE2], [], [Use pipe2 for SOCK_CLOEXEC])
	fi

	dnl chain build if thirdparty/c-ares exists, otherwise search for it
	if test -d "${srcdir}/thirdparty/c-ares" ; then
		dnl Tell c-ares to only build static if chain building
		ac_configure_args="${ac_configure_args} --disable-shared --enable-static --with-pic"
		AC_CONFIG_SUBDIRS([thirdparty/c-ares])
		BUILD_SUBDIRS="${BUILD_SUBDIRS} thirdparty/c-ares"
		C_ARES_CPPFLAGS='-I$(top_srcdir)/thirdparty/c-ares -I$(top_builddir)/thirdparty/c-ares'
		C_ARES_LIBADD='$(top_builddir)/thirdparty/c-ares/libcares.la'
		C_ARES_LDFLAGS=""
		AC_SUBST(C_ARES_CPPFLAGS)
		AC_SUBST(C_ARES_LIBADD)
		AC_SUBST(C_ARES_LDFLAGS)
	else
		AX_FIND_PACKAGE(c-ares,
			[/usr/local/cares /usr/local/c-ares /usr/local],
			[ares.h],
			[ares_init],
			[cares])

		if test "${C_ARES_FOUND}" != "yes" ; then
			AC_MSG_ERROR(unable to locate c-ares)
		fi
	fi

	BUILD_SUBDIRS="${BUILD_SUBDIRS} io"
fi


dnl
dnl TLS
dnl
AC_ARG_ENABLE(mstdlib-tls,
	AC_HELP_STRING([--disable-mstdlib-tls], [Disable building of tls library (and any dependent libraries)]),
	[ build_mstdlib_tls=${enableval} ],
	[ build_mstdlib_tls=${build_mstdlib_io} ])

if test "${build_mstdlib_tls}" = "yes" ; then
	if test "${build_mstdlib_io}" = "no" ; then
		AC_MSG_ERROR(TLS REQUIRES IO)
	fi

	if test "$INTERNAL_BUILD" = "yes" ; then
		ssl_dirs="/usr/local/ssl-purify /usr/local/ssl /usr/local /usr"
	else
		ssl_dirs="/usr /usr/local /usr/local/ssl"
	fi
	AC_ARG_WITH(ssl-dir,
		AC_HELP_STRING([--with-ssl-dir=<dir>], [Paths to search for openssl (default=$ssl_dirs)]),
		[ ssl_dirs=$withval ],
		[ ])

	AC_MSG_CHECKING(for ssl library location)
	ssl_dir=""
	for dir in $ssl_dirs ; do
		exts="so sl dylib a"
		for ext in $exts ; do
			if test -r "$dir/lib/libssl.${ext}" ; then
				ssl_header_dir="$dir/include"
				ssl_lib_dir="$dir/lib"
				ssl_ext=$ext
				break
			elif test -r "$dir/lib/x86_64-linux-gnu/libssl.${ext}" ; then
				ssl_header_dir="$dir/include"
				ssl_lib_dir="$dir/lib/x86_64-linux-gnu"
				ssl_ext=$ext
				break
			elif test -r "$dir/lib/x86-linux-gnu/libssl.${ext}" ; then
				ssl_header_dir="$dir/include"
				ssl_lib_dir="$dir/lib/x86-linux-gnu"
				ssl_ext=$ext
				break
			elif test -r "$dir/lib64/libssl.${ext}" ; then
				ssl_header_dir="$dir/include"
				ssl_lib_dir="$dir/lib64"
				ssl_ext=$ext
				break
			fi
		done
		if test "x$ssl_lib_dir" != "x" ; then
			break
		fi
	done
	if test "x$ssl_lib_dir" = "x" ; then
		AC_MSG_ERROR(OpenSSL not found)
	fi
	AC_MSG_RESULT($ssl_lib_dir)


	AC_ARG_ENABLE(ssl-static,
		AS_HELP_STRING([--enable-ssl-static],
		               [statically link against OpenSSL (default disabled)]),
		[
			ssl_static=$enableval
			AC_MSG_RESULT(--enable-ssl-static specified as $enableval)
		], [
			AC_MSG_RESULT(--enable-ssl-static NOT SPECIFIED)
			if test "x$ssl_ext" = "xa" ; then
				ssl_static=yes
			else
				ssl_static=no
			fi
		])

	if test "$ssl_static" = "yes" ; then
		SSL_LIBS=""  # $ssl_lib_dir/libssl.a $ssl_lib_dir/libcrypto.a"
	else
		SSL_LIBS="-lssl -lcrypto"
	fi

	LDFLAGS="${LDFLAGS} -L$ssl_lib_dir"
	CPPFLAGS="${CPPFLAGS} -I$ssl_header_dir"
	AC_SUBST(SSL_LIBS)

	BUILD_SUBDIRS="${BUILD_SUBDIRS} tls"
fi

dnl
dnl log
dnl
AC_ARG_ENABLE(mstdlib-log,
	AC_HELP_STRING([--disable-mstdlib-log], [Disable building of mstdlib log subsystem]),
	[ build_mstdlib_log=${enableval} ],
	[ build_mstdlib_log=yes ])

if test "${build_mstdlib_log}" = "yes" ; then
	if test "${build_mstdlib_io}" = "no" ; then
		AC_MSG_ERROR(LOG REQUIRES IO)
	fi
	BUILD_SUBDIRS="${BUILD_SUBDIRS} log"
fi


dnl
dnl sql
dnl
AC_ARG_ENABLE(mstdlib-sql,
	AC_HELP_STRING([--disable-mstdlib-sql], [Disable building of mstdlib sql subsystem]),
	[ build_mstdlib_sql=${enableval} ],
	[ build_mstdlib_sql=yes ])

if test "${build_mstdlib_sql}" = "yes" ; then
	if test "${build_mstdlib_io}" = "no" ; then
		AC_MSG_ERROR(SQL REQUIRES IO)
	fi
	BUILD_SUBDIRS="${BUILD_SUBDIRS} sql"

	AC_CHECK_LIB([dl], [dlopen], [MSTDLIB_SQL_LIBADD="-ldl"], [])
	AC_SUBST(MSTDLIB_SQL_LIBADD)

	AC_ARG_ENABLE(mstdlib-sql-sqlite,
		AC_HELP_STRING([--enable-mstdlib-sql-sqlite], [Enable building of mstdlib sql sqlite plugin]),
		[ build_mstdlib_sql_sqlite=${enableval} ],
		[ build_mstdlib_sql_sqlite=maybe ])

	AC_ARG_ENABLE(mstdlib-sql-mysql,
		AC_HELP_STRING([--enable-mstdlib-sql-mysql], [Enable building of mstdlib sql mysql plugin]),
		[ build_mstdlib_sql_mysql=${enableval} ],
		[ build_mstdlib_sql_mysql=maybe ])

	AC_ARG_ENABLE(mstdlib-sql-postgresql,
		AC_HELP_STRING([--enable-mstdlib-sql-postgresql], [Enable building of mstdlib sql postgresql plugin]),
		[ build_mstdlib_sql_postgresql=${enableval} ],
		[ build_mstdlib_sql_postgresql=maybe ])

	AC_ARG_ENABLE(mstdlib-sql-oracle,
		AC_HELP_STRING([--enable-mstdlib-sql-oracle], [Enable building of mstdlib sql oracle plugin]),
		[ build_mstdlib_sql_oracle=${enableval} ],
		[ build_mstdlib_sql_oracle=maybe ])

	AC_ARG_ENABLE(mstdlib-sql-odbc,
		AC_HELP_STRING([--enable-mstdlib-sql-odbc], [Enable building of mstdlib sql odbc plugin]),
		[ build_mstdlib_sql_odbc=${enableval} ],
		[ build_mstdlib_sql_odbc=maybe ])


	if test "${build_mstdlib_sql_sqlite}" != "no" ; then
		dnl chain build if thirdparty/sqlite-amalgamation exists, otherwise search for it
		if test -d "${srcdir}/thirdparty/sqlite-amalgamation" ; then
			SQLITE_CPPFLAGS='-I$(top_srcdir)/thirdparty/sqlite-amalgamation -DSQLITE_MUTEX_APPDEF=1 -DSQLITE_THREADSAFE=1 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS=1 -DSQLITE_MAX_EXPR_DEPTH=0 -DSQLITE_OMIT_PROGRESS_CALLBACK=1 -DSQLITE_OMIT_DEPRECATED=1 -DSQLITE_MAX_VARIABLE_NUMBER=32768'
			SQLITE_LIBADD=""
			SQLITE_LDFLAGS=""
			AC_SUBST(SQLITE_CPPFLAGS)
			AC_SUBST(SQLITE_LIBADD)
			AC_SUBST(SQLITE_LDFLAGS)
			SQLITE_FOUND=yes
		else
			AX_FIND_PACKAGE(sqlite,
				[/usr/local/sqlite /usr/local],
				[sqlite3.h],
				[sqlite3_open_v2],
				[sqlite3],
				[],
				[/include/sqlite /include/sqlite3 /include /],
				[/lib/sqlite /lib/sqlite3 /lib /])
		fi

		if test "${SQLITE_FOUND}" = "yes" ; then
			BUILD_SUBDIRS="${BUILD_SUBDIRS} sql/sqlite"
		else
			if test "${build_mstdlib_sql_sqlite}" = "yes" ; then
				AC_MSG_ERROR(sqlite requested but not found)
			fi
		fi
		AM_CONDITIONAL([SQLITE_AMALGAMATION], [ test -d "${srcdir}/thirdparty/sqlite-amalgamation" ])
	fi

	if test "${build_mstdlib_sql_mysql}" != "no" ; then
		AX_FIND_PACKAGE(mysql,
			[/usr/local/mariadb /usr/local/mysql /usr/local],
			[mysql.h],
			[mysql_init],
			[mariadb mysqlclient_r mysqlclient],
			[],
			[/include/mariadb /include/mysql /include /],
			[/lib/mariadb /lib/mysql /lib /])

		if test "${MYSQL_FOUND}" = "yes" ; then
			BUILD_SUBDIRS="${BUILD_SUBDIRS} sql/mysql"
		else
			if test "${build_mstdlib_sql_mysql}" = "yes" ; then
				AC_MSG_ERROR(mysql requested but not found)
			fi
		fi
	fi

	if test "${build_mstdlib_sql_postgresql}" != "no" ; then
		AX_FIND_PACKAGE(postgresql,
			[/usr/local/postgresql /usr/local/pgsql /usr/local /usr/lib/postgresql/9.5 /usr/include/postgresql],
			[libpq-fe.h],
			[PQconnectdbParams],
			[pq],
			[],
			[/include/pgsql /include/postgresql /include /],
			[/lib/pgsql /lib/postgresql /lib /])

		if test "${POSTGRESQL_FOUND}" = "yes" ; then
			BUILD_SUBDIRS="${BUILD_SUBDIRS} sql/postgresql"
		else
			if test "${build_mstdlib_sql_postgresql}" = "yes" ; then
				AC_MSG_ERROR(postgresql requested but not found)
			fi
		fi
	fi

	if test "${build_mstdlib_sql_oracle}" != "no" ; then
		AX_FIND_PACKAGE(oracle,
			[/usr/local/oracle],
			[oci.h],
			[OCIEnvNlsCreate],
			[clntsh])

		if test "${ORACLE_FOUND}" = "yes" ; then
			BUILD_SUBDIRS="${BUILD_SUBDIRS} sql/oracle"
		else
			if test "${build_mstdlib_sql_oracle}" = "yes" ; then
				AC_MSG_ERROR(oracle requested but not found)
			fi
		fi
	fi

	if test "${build_mstdlib_sql_odbc}" != "no" ; then
		AX_FIND_PACKAGE(odbc,
			[/usr/local/unixodbc /usr/local/iodbc],
			[sql.h],
			[SQLAllocHandle],
			[odbc])

		if test "${ODBC_FOUND}" = "yes" ; then
			BUILD_SUBDIRS="${BUILD_SUBDIRS} sql/odbc"
		else
			if test "${build_mstdlib_sql_odbc}" = "yes" ; then
				AC_MSG_ERROR(odbc requested but not found)
			fi
		fi
	fi

fi

dnl
dnl tests
dnl
AC_ARG_ENABLE(tests,
	AC_HELP_STRING([--disable-tests], [Disable building of tests]),
	[ build_tests=${enableval} ],
	[ build_tests=yes ])

if test "${build_tests}" = "yes" ; then
	BUILD_SUBDIRS="${BUILD_SUBDIRS} test"
fi

dnl Needed for determining which tests to build
AM_CONDITIONAL([MSTDLIB_FORMATS],     [ test $build_mstdlib_formats = yes ])
AM_CONDITIONAL([MSTDLIB_THREAD],      [ test $build_mstdlib_thread = yes ])
AM_CONDITIONAL([MSTDLIB_IO],          [ test $build_mstdlib_io = yes ])
AM_CONDITIONAL([MSTDLIB_TLS],         [ test $build_mstdlib_tls = yes ])
AM_CONDITIONAL([MSTDLIB_LOG],         [ test $build_mstdlib_log = yes ])
AM_CONDITIONAL([MSTDLIB_SQL],         [ test $build_mstdlib_sql = yes ])

AC_DEFINE([ADD_OBJECTS], [], [Additional sources])
AC_SUBST(ADD_OBJECTS)
AC_SUBST(BUILD_SUBDIRS)


dnl Libtool versioning is weird, they use a 'current', 'age', and 'revision'
dnl methodology to determine the ABI version.  We actually are guaranteeing
dnl it based on our versions.  Major means ABI breaks, minor means addition
dnl of new functions, and release means bugfixes.  We need to map our version
dnl to the libtool version.  The forward method for .so versioning is
dnl (current-age).age.revision, so we'll do the opposite here:
LIBTOOL_VERSION="`expr mstdlib_major_version + mstdlib_minor_version`:`echo mstdlib_release_version`:`echo mstdlib_minor_version`"
AC_SUBST(LIBTOOL_VERSION)

AC_OUTPUT
