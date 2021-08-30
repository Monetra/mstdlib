# SYNOPSIS
#
#   AX_FIND_PACKAGE(<name>, <search paths>, <header>, <function>, <libs>, [<extra libs>], [<header_dir_suffix>], [<lib_dir_suffix>]).
#
# DESCRIPTION
#
#   Check for a package (headers + library) and return:
#      NAME_FOUND=yes/no
#   If found, these variables will also be set and substituted via AC_SUBST():
#      NAME_CPPFLAGS
#      NAME_LDFLAGS
#      NAME_LIBADD
#
#   The NAME is the package name specified converted to UPPERCASE. Hiphens will be
#   converted to Underscores.
#
#   <name>              is a user-specified package name
#   <search paths>      is a space delimited list of installation prefixes to
#                       search for the package
#   <header>            is a header to locate (e.g. mysql.h)
#   <function>          function name found in library.
#   <libs>              is a space delimited list of possible library names,
#                       only one will be found (e.g. mariadb mysqlclient)
#   <extra libs>        Optional. is a set of linker flags specifying other
#                       libraries that may need to be included (e.g. -lz)
#   <header_dir_suffix> Optional. space delimited list of header suffix
#                       directories to search.
#                       (e.g. include/mysql include/mariadb)
#                       If not specified, defaults to [/include /]
#   <lib_dir_suffix>    Optional. space delimited list of library suffix
#                       directories to search. (e.g. lib/mariadb lib/mysql)
#                       If not specified, defaults to [/lib /]
#
#
#   An additional configuration argument of --with-name-dir= will be added to allow
#   the caller to specify their desired search path list.
#
#   Any libraries specified in <extra libs> will be appended to the NAME_LIBADD.
#
#   Example:
#
#    AX_FIND_PACKAGE(openssl, [/usr/local/ssl /usr/local/openssl /usr/local], [ssl.h], [EVP_EncryptInit], [crypto])
#
#    might result in shell variables being set:
#
#    OPENSSL_FOUND=yes
#    OPENSSL_CPPFLAGS=-I/usr/local/ssl/include
#    OPENSSL_LDFLAGS=-Wl,-rpath-link,/usr/local/ssl/lib -L/usr/local/ssl/lib
#    OPENSSL_LIBADD=-lcrypto
#
#    or if not found:
#
#    OPENSSL_FOUND=no
#
#serial 2

AC_DEFUN([AX_FIND_PACKAGE],
	[
		ax_name=`echo $1 | $as_tr_sh`
		ax_pkg_name=`echo $ax_name | tr '[a-z]' '[A-Z]'`
		ax_search_paths="$2"
		ax_header=$3
		ax_header_name=`echo $3 | $as_tr_sh`
		ax_function=$4
		ax_libs="$5"
		ax_extra_libs="$6"
		ax_header_suffix="$7"
		ax_lib_suffix="$8"
		ax_found=no

		AC_ARG_WITH([$1-dir],
			AS_HELP_STRING([--with-$1-dir=<paths>], [$1 installation prefix list, space separated (default: $2)]),
			[ ax_search_paths="$withval" ])

		if test "${ax_header_suffix}" = "" ; then
			ax_header_suffix="/include /"
		fi

		if test "${ax_lib_suffix}" = "" ; then
			ax_lib_suffix="/lib /"
		fi

		AS_VAR_PUSHDEF([ax_package_found], [${ax_pkg_name}_FOUND])
		AS_VAR_SET([ax_package_found], [no])

		AX_EXPAND_SET([ax_include_search], [${ax_search_paths}], [${ax_header_suffix}])
		AX_FIND_HEADER([${ax_header}],[${ax_include_search}])

		dnl Only search for library if header was found
		if eval `echo 'test x${'have_${ax_header_name}'}' = "xyes"`; then
			AX_EXPAND_SET([ax_lib_search], [${ax_search_paths}], [${ax_lib_suffix}])
			AX_FIND_LIBRARY([${ax_name}], [${ax_libs}], [${ax_function}], [${ax_lib_search}], [${ax_extra_libs}])

			dnl If library is found, push all variables
			if eval `echo 'test x${'have_lib_${ax_name}'}' = "xyes"`; then
				ax_found=yes

				AS_VAR_SET([ax_package_found], [yes])

				eval "ax_header_path=`echo '${'have_${ax_header_name}_path'}'`"
				AS_VAR_PUSHDEF([ax_package_cppflags], [${ax_pkg_name}_CPPFLAGS])
				if test "$ax_header_path" != "" ; then
					AS_VAR_SET([ax_package_cppflags], [-I${ax_header_path}])
				fi
				m4_pushdef([AX_AC_SUBST], m4_translit([$1], [-a-z], [_A-Z])_CPPFLAGS)
				AC_SUBST(AX_AC_SUBST)
				m4_popdef([AX_AC_SUBST])
				AS_VAR_POPDEF([ax_package_cppflags])


				eval "ax_lib_path=`echo '${'have_lib_${ax_name}_path'}'`"
				AS_VAR_PUSHDEF([ax_package_ldflags], [${ax_pkg_name}_LDFLAGS])
				if test "$ax_lib_path" != "" ; then
					AS_VAR_SET([ax_package_ldflags], [-L${ax_lib_path}])
					if test "$ax_find_library_have_rpath_link" = "yes" ; then
						AS_VAR_APPEND([ax_package_ldflags], [" -Wl,-rpath-link,${ax_lib_path}"])
					fi
				fi
				m4_pushdef([AX_AC_SUBST], m4_translit([$1], [-a-z], [_A-Z])_LDFLAGS)
				AC_SUBST(AX_AC_SUBST)
				m4_popdef([AX_AC_SUBST])
				AS_VAR_POPDEF([ax_package_ldflags])


				eval "ax_lib_lib=\"`echo -l'${'have_lib_${ax_name}_lib'}' '${ax_extra_libs}`\""
				AS_VAR_PUSHDEF([ax_package_libadd], [${ax_pkg_name}_LIBADD])

				AS_VAR_SET([ax_package_libadd], [${ax_lib_lib}])
				m4_pushdef([AX_AC_SUBST], m4_translit([$1], [-a-z], [_A-Z])_LIBADD)
				AC_SUBST(AX_AC_SUBST)
				m4_popdef([AX_AC_SUBST])
				AS_VAR_POPDEF([ax_package_libadd])
			fi
		fi

		m4_pushdef([AX_AC_SUBST], m4_translit([$1], [-a-z], [_A-Z])_FOUND)
		AC_SUBST([AX_AC_SUBST])
		m4_popdef([AX_AC_SUBST])

		AS_VAR_POPDEF([ax_package_found])

		AC_MSG_CHECKING(for package ${ax_name})
		AC_MSG_RESULT(${ax_found})
	])
