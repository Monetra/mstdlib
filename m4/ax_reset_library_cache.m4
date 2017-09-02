# SYNOPSIS
#
# AX_RESET_LIBRARY_CACHE(library function)
#
# DESCRIPTION
#
# This macro invalidates the library cache variables created by previous AC_CHECK_LIB checks.
#
AC_DEFUN([AX_RESET_LIBRARY_CACHE], [
	AS_VAR_PUSHDEF([ax_Var], [ac_cv_lib_${1}_${2}])
	AS_UNSET([ax_Var])
	AS_VAR_POPDEF([ax_Var])
]) # AX_RESET_LIBRARY_CACHE
