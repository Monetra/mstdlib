# SYNOPSIS
#
#   AX_FIND_HEADER(<header>, <paths>).
#
# DESCRIPTION
#
#   Check for <header> with -I<path> for each path in <paths> if need be.
#   The first successful path is chosen.  Paths are space separated.  Note
#   that if the header exists in the system path, it may return the wrong
#   path.
#
#   Will define have_${header}=yes and have_${header}_path=$path
#
#   Example:
#
#    AX_FIND_HEADER(openssl/rsa.h, /usr/local/include /usr/local/ssl/include /usr/local/openssl/include)
#
#    might result in shell variables being set:
#
#    have_openssl_rsa_h=yes
#    have_openssl_rsa_h_path=/usr/local/ssl/include
#
#    or if not found:
#
#    have_openssl_rsa_h=no
#
#serial 1

AC_DEFUN([AX_FIND_HEADER],
	[AC_LANG_PUSH(C)
		hdr=`echo $1 | $as_tr_sh`
		ax_find_header_found="no"
		ax_find_header_found_path=""
		AC_MSG_CHECKING(for $1)
		for dir in $2; do
			if test ! -d "$dir" ; then
				continue;
			fi

			ax_find_header_CPPFLAGS="${CPPFLAGS}"
			CPPFLAGS="${CPPFLAGS} -I$dir"
			AC_COMPILE_IFELSE(
				[AC_LANG_PROGRAM([#include <$1>])],
				[
				  ax_find_header_found="yes"
				  ax_find_header_found_path="${dir}"
				],
				[ax_find_header_found="no"])
			CPPFLAGS="${ax_find_header_CPPFLAGS}"

			if test "$ax_find_header_found" = "yes" ; then
				break
			fi
		done

		dnl check system only
		if test "$ax_find_header_found" = "no" ; then
			AC_COMPILE_IFELSE(
				[AC_LANG_PROGRAM([#include <$1>])],
				[
				  ax_find_header_found="yes"
				  ax_find_header_found_path=""
				],
				[ax_find_header_found="no"])
		fi

		if test "$ax_find_header_found" = "yes" ; then
			AS_VAR_PUSHDEF([ax_Var], [have_${hdr}])
			AS_VAR_SET([ax_Var], ["yes"])
			AS_VAR_POPDEF([ax_Var])

			AS_VAR_PUSHDEF([ax_Var], [have_${hdr}_path])
			AS_VAR_SET([ax_Var], ["${ax_find_header_found_path}"])
			AS_VAR_POPDEF([ax_Var])

			if test "${ax_find_header_found_path}" = "" ; then
				ax_find_header_found_path="system"
			fi

			AC_MSG_RESULT(${ax_find_header_found_path})
		else
			AS_VAR_PUSHDEF([ax_Var], [have_${hdr}])
			AS_VAR_SET([ax_Var], ["no"])
			AS_VAR_POPDEF([ax_Var])
			AC_MSG_RESULT(not found)
		fi

	AC_LANG_POP])
