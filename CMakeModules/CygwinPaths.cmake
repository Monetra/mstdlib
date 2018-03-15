# CONVERT_CYGWIN_PATH( PATH )
#  This uses the command cygpath (provided by cygwin) to convert
#  unix-style paths into paths useable by cmake on windows

if (WIN32)
	find_program(CYGPATH
		NAMES cygpath
		
		HINTS [HKEY_LOCAL_MACHINE\\Software\\Cygwin\\setup;rootdir]/bin
	    
		PATHS C:/cygwin64/bin
		      C:/cygwin/bin
	)
endif ()

function(convert_cygwin_path _pathvar)
	if (WIN32 AND CYGPATH)
		execute_process(
			COMMAND         "${CYGPATH}" -m "${${_pathvar}}"
			OUTPUT_VARIABLE ${_pathvar}
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		set(${_pathvar} "${${_pathvar}}" PARENT_SCOPE)
	endif ()
endfunction()

function(convert_windows_path _pathvar)
	if (CYGPATH)
		execute_process(
			COMMAND         "${CYGPATH}" "${${_pathvar}}"
			OUTPUT_VARIABLE ${_pathvar}
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		set(${_pathvar} "${${_pathvar}}" PARENT_SCOPE)
	endif ()
endfunction()
