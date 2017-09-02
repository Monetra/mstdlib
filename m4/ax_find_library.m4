# SYNOPSIS
#
#   AX_FIND_LIBRARY(<name>, <lib names>, <function>, <paths>, <extra libs>).
#
# DESCRIPTION
#
#   Check for the existance of one of the lib names with -L<path> for each path in
#   <paths> if need be. The first successful path is chosen.  Paths are space
#   separated.  Note that if the library exists in the system path, it may return
#   the wrong path.
#
#   If extra libraries are needed, they may be specified in <extra libs>
#
#   Will define:
#       have_lib_${name}=yes/no
#       have_lib_${name}_path=$path
#       have_lib_${name}_lib=one of the lib names
#
#   Example:
#
#    AX_FIND_LIBRARY([mysql], [mysqlclient_r mysqlclient mariadb], [mysql_init], [/usr/local/lib /usr/local/mysql/lib /usr/local/mysql/lib/mysql], [])
#
#    might result in shell variables being set:
#
#    have_lib_mysql=yes
#    have_lib_mysql_path=/usr/local/mysql/lib/mysql
#    have_lib_mysql_lib=libmysqlclient_r
#
#    or if not found:
#
#    have_lib_mysql=no
#
#serial 1

AC_DEFUN([AX_FIND_LIBRARY],
	[AC_LANG_PUSH(C)
		ax_name=`echo $1 | $as_tr_sh`
		if test -z "$ax_find_library_have_rpath_link" ; then
			AX_CHECK_LINK_FLAG([-Wl,-rpath-link,/usr], [ax_find_library_have_rpath_link="yes"], [ax_find_library_have_rpath_link="no"])
		fi

		ax_find_library_found=no
		ax_find_library_found_path=""
		ax_find_library_found_lib=""
		AC_MSG_CHECKING(for library $1)
		for dir in $4; do
			if test ! -d "$dir" ; then
				continue;
			fi

			for lib in $2; do
				ax_find_library_LDFLAGS="${LDFLAGS}"
				ax_find_library_LIBS="${LIBS}"
				LDFLAGS="${LDFLAGS} -L${dir}"
				if test "$ax_find_library_have_rpath_link" = "yes" ; then
					LDFLAGS="${LDFLAGS} -Wl,-rpath-link,${dir}"
				fi
				LIBS="-l${lib} $5"
				AC_TRY_LINK_FUNC([$3],
					[
					  ax_find_library_found="yes"
					  ax_find_library_found_path="${dir}"
					  ax_find_library_found_lib="${lib}"
					],
					[ax_find_library_found="no"])
				LDFLAGS="${ax_find_library_LDFLAGS}"
				LIBS="${ax_find_library_LIBS}"

				if test "$ax_find_library_found" = "yes" ; then
					break
				fi

			done

			if test "$ax_find_library_found" = "yes" ; then
				break
			fi
		done

		dnl check system only
		if test "$ax_find_library_found" = "no" ; then
			for lib in $2; do
				ax_find_library_LIBS="${LIBS}"
				LIBS="-l${lib} $5"
				AC_TRY_LINK_FUNC([$3],
					[
					  ax_find_library_found="yes"
					  ax_find_library_found_path=""
					  ax_find_library_found_lib="${lib}"
					],
					[ax_find_library_found="no"])
				LIBS="${ax_find_library_LIBS}"

				if test "$ax_find_library_found" = "yes" ; then
					break
				fi
			done
		fi

		if test "$ax_find_library_found" = "yes" ; then
			AS_VAR_PUSHDEF([ax_Var], [have_lib_${ax_name}])
			AS_VAR_SET([ax_Var], ["yes"])
			AS_VAR_POPDEF([ax_Var])

			AS_VAR_PUSHDEF([ax_Var], [have_lib_${ax_name}_path])
			AS_VAR_SET([ax_Var], ["${ax_find_library_found_path}"])
			AS_VAR_POPDEF([ax_Var])

			AS_VAR_PUSHDEF([ax_Var], [have_lib_${ax_name}_lib])
			AS_VAR_SET([ax_Var], ["${ax_find_library_found_lib}"])
			AS_VAR_POPDEF([ax_Var])

			if test "${ax_find_library_found_path}" = "" ; then
				ax_find_header_found_path="system"
			fi

			AC_MSG_RESULT([${ax_find_library_found_path}(${ax_find_library_found_lib})])
		else
			AS_VAR_PUSHDEF([ax_Var], [have_lib_${ax_name}])
			AS_VAR_SET([ax_Var], ["no"])
			AS_VAR_POPDEF([ax_Var])
			AC_MSG_RESULT(not found)
		fi

	AC_LANG_POP])
