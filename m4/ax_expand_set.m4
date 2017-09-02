# SYNOPSIS
#
# AX_EXPAND_SET(outvar prefix suffix)
#
# DESCRIPTION
#
# This macro expands a list of prefixes appending a suffix to each prefix and outputs
# the result into the specified variable
#
# Example:
#   AX_EXPAND_SET([mylist], [/usr/local/mysql /usr/local/mariadb /usr/local], [/include /include/mysql])
# Result might be
#   mylist=/usr/local/mysql/include /usr/local/mysql/include/mysql /usr/local/mariadb/include /usr/local/mariadb/include/mysql
#
# serial 1

AC_DEFUN([AX_EXPAND_SET],
	[
		AS_VAR_PUSHDEF([ax_Var], [$1])
		AS_VAR_SET([ax_Var], [])
		for ax_prefix in $2; do
			for ax_suffix in $3; do
				AS_VAR_APPEND([ax_Var], [" ${ax_prefix}${ax_suffix}"])
			done
		done
		AS_VAR_POPDEF([ax_Var])
	])

