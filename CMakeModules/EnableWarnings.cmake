# EnableWarnings.cmake
#
# Checks for and turns on a large number of warning C flags.
#
# Adds the following helper functions:
#
#	remove_warnings(... list of warnings ...)
#		Turn off given list of individual warnings for all targets and subdirectories added after this.
#
#   remove_all_warnings()
#		Remove all warning flags, add -w to suppress built-in warnings.
#
#   remove_all_warnings_from_targets(... list of targets ...)
#       Suppress warnings for the given targets only.
#
#   push_warnings()
#		Save current warning flags by pushing them onto an internal stack. Note that modifications to the internal
#		stack are only visible in the current CMakeLists.txt file and its children.
#
#       Note: changing warning flags multiple times in the same directory only affects add_subdirectory() calls.
#             Targets in the directory will always use the warning flags in effect at the end of the CMakeLists.txt
#             file - this is due to really weird and annoying legacy behavior of CMAKE_C_FLAGS.
#
#   pop_warnings()
#       Restore the last set of flags that were saved with push_warnings(). Note that modifications to the internal
#		stack are only visible in the current CMakeLists.txt file and its children.
#

if (_internal_enable_warnings_already_run)
	return()
endif ()
set(_internal_enable_warnings_already_run TRUE)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)

# internal helper: _int_enable_warnings_set_flags(langs_var [warnings flags])
function(_int_enable_warnings_set_flags langs_var)
	foreach(_flag ${ARGN})
		string(MAKE_C_IDENTIFIER "HAVE_${_flag}" varname)

		if ("C" IN_LIST ${langs_var})
			check_c_compiler_flag(${_flag} ${varname})
			if (${varname})
				string(APPEND CMAKE_C_FLAGS " ${_flag}")
			endif ()
		endif ()

		if ("CXX" IN_LIST ${langs_var})
			string(APPEND varname "_CXX")
			check_cxx_compiler_flag(${_flag} ${varname})
			if (${varname})
				string(APPEND CMAKE_CXX_FLAGS " ${_flag}")
			endif ()
		endif ()
	endforeach()

	foreach(lang C CXX)
		string(STRIP "${CMAKE_${lang}_FLAGS}" CMAKE_${lang}_FLAGS)
		set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}" PARENT_SCOPE)
	endforeach()
endfunction()

if (MSVC)
	# Visual Studio uses a completely different nomenclature for warnings than gcc/mingw/clang, so none of the
	# "-W[name]" warnings will work.

	# W4 would be better but it produces unnecessary warnings like:
	# *  warning C4706: assignment within conditional expression
	#     Triggered when doing "while(1)"
	# * warning C4115: 'timeval' : named type definition in parentheses
	# * warning C4201: nonstandard extension used : nameless struct/union
	#     Triggered by system includes (commctrl.h, shtypes.h, Shlobj.h)
	set(_flags
		/W3
		/we4013 # Treat "function undefined, assuming extern returning int" warning as an error. https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4013
	)

	# Disable some warnings to reduce noise level on visual studio.
	if (NOT WIN32_STRICT_WARNINGS)
		list(APPEND _flags
			/wd4018 # Disable signed/unsigned mismatch warnings. https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4018
			/wd4068 # Disable unknown pragma warning. https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-1-c4068
			/wd4244 # Disable integer type conversion warnings. https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-levels-3-and-4-c4244
			/wd4267 # Disable warnings about converting size_t to a smaller type. https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4267
		)
	endif ()

	set(_cxx_flags "${_flags}")
else ()
	# If we're compiling with GCC / Clang / MinGW (or anything else besides Visual Studio):
	# C Flags:
	set(_flags
		-W -Wall -Wextra -Wchar-subscripts -Wcomment -Wno-coverage-mismatch
		-Wdouble-promotion -Wformat -Wnonnull -Winit-self
		-Wignored-qualifiers -Wmain
		-Wmissing-braces -Wmissing-include-dirs -Wparentheses -Wsequence-point
		-Wreturn-type -Wswitch -Wtrigraphs -Wunused-but-set-parameter
		-Wunused-but-set-variable -Wunused-function -Wunused-label
		-Wunused-local-typedefs -Wunused-parameter -Wunused-variable -Wunused-value
		-Wunused -Wuninitialized -Wmaybe-uninitialized -Wunknown-pragmas
		-Wmissing-format-attribute -Warray-bounds
		-Wtrampolines -Wfloat-equal
		-Wdeclaration-after-statement -Wundef -Wshadow
		-Wpointer-arith -Wtype-limits
		-Wcast-align -Wwrite-strings -Wclobbered -Wempty-body
		-Wenum-compare -Wjump-misses-init -Wsign-compare -Wsizeof-pointer-memaccess
		-Waddress -Wlogical-op
		-Wstrict-prototypes -Wold-style-declaration -Wold-style-definition
		-Wmissing-parameter-type -Wmissing-prototypes -Wmissing-declarations
		-Wmissing-field-initializers -Woverride-init -Wpacked -Wredundant-decls
		-Wnested-externs -Winvalid-pch -Wvariadic-macros -Wvarargs
		-Wvla -Wpointer-sign -Wdisabled-optimization -Wendif-labels -Wpacked-bitfield-compat
		-Wformat-security -Woverlength-strings -Wstrict-aliasing
		-Wstrict-overflow -Wsync-nand -Wvolatile-register-var
		-Wconversion -Wsign-conversion

		# Treat implicit variable typing and implicit function declarations as errors.
		-Werror=implicit-int
		-Werror=implicit-function-declaration

		# Make MacOSX honor -mmacosx-version-min
		-Werror=partial-availability
	)

	# C++ flags:
	set(_cxx_flags
		-Wall
		-Wextra
		-Wcast-align
		-Wmissing-declarations
		-Wredundant-decls
		-Wformat
		-Wmissing-format-attribute
		-Wformat-security
		-Wtype-limits
		-Wcast-align
		-Winvalid-pch
		-Wvarargs
		-Wvla
		-Wendif-labels
		-Wpacked-bitfield-compat

		-Wno-unused-parameter
	)

	# Note: when cross-compiling to Windows from Cygwin, the Qt Mingw packages have a bunch of
	#       noisy type-conversion warnings in headers. So, only enable those warnings if we're
	#       not building that configuration.

	if (NOT (WIN32 AND (CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN")))
		list(APPEND _cxx_flags
			-Wsign-conversion
			-Wconversion
			-Wfloat-equal
		)
	endif ()
endif ()

# Check and set C compiler flags.
set(lang "C")
if (lang IN_LIST languages)
	_int_enable_warnings_set_flags(lang ${_flags})
endif ()

# Check and set C++ compiler flags (if C++ language is enabled).
set(lang "CXX")
if (lang IN_LIST languages)
	_int_enable_warnings_set_flags(lang ${_cxx_flags})
endif ()

# Add flags to force output colors.
if (CMAKE_GENERATOR MATCHES "Ninja")
	set(color_default TRUE)
else ()
	set(color_default FALSE)
endif ()
option(FORCE_COLOR "Force compiler to always colorize, even when output is redirected." ${color_default})
mark_as_advanced(FORCE FORCE_COLOR)
if (FORCE_COLOR)
	_int_enable_warnings_set_flags(languages
		-fdiagnostics-color=always # GCC
		-fcolor-diagnostics        # Clang
	)
endif ()



# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Helper functions


# This function can be called in subdirectories, to prune out warnings that they don't want.
#  vararg: warning flags to remove from list of enabled warnings. All "no" flags after EXPLICIT_DISABLE
#          will be added to C flags.
#
# Ex.: remove_warnings(-Wall -Wdouble-promotion -Wcomment) prunes those warnings flags from the compile command.
function(remove_warnings)
	get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	set(langs C)
	if ("CXX" IN_LIST languages)
		list(APPEND langs CXX)
	endif ()

	foreach(lang ${langs})
		set(toadd)
		set(in_explicit_disable FALSE)
		foreach (flag ${ARGN})
			if (flag STREQUAL "EXPLICIT_DISABLE")
				set(in_explicit_disable TRUE)
			elseif (in_explicit_disable)
				list(APPEND toadd "${flag}")
			else ()
				string(REGEX REPLACE "${flag}([ \t]+|$)" "" CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}")
			endif ()
		endforeach ()
		_int_enable_warnings_set_flags(lang ${toadd})
		string(STRIP "${CMAKE_${lang}_FLAGS}" CMAKE_${lang}_FLAGS)
		set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}" PARENT_SCOPE)
	endforeach()
endfunction()


# Explicitly suppress all warnings. As long as this flag is the last warning flag, warnings will be
# suppressed even if earlier flags enabled warnings.
function(remove_all_warnings)
	get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	set(langs C)
	if ("CXX" IN_LIST languages)
		list(APPEND langs CXX)
	endif ()

	foreach(lang ${langs})
		string(REGEX REPLACE "[-/][Ww][^ \t]*([ \t]+|$)" "" CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}")
		if (MSVC)
			string(APPEND CMAKE_${lang}_FLAGS " /w")
		else ()
			string(APPEND CMAKE_${lang}_FLAGS " -w")
		endif ()
		string(STRIP "${CMAKE_${lang}_FLAGS}" CMAKE_${lang}_FLAGS)
		set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}" PARENT_SCOPE)
	endforeach()
endfunction()


function(remove_all_warnings_from_targets)
	foreach (target ${ARGN})
		if (MSVC)
			target_compile_options(${target} PRIVATE "/w")
		else ()
			target_compile_options(${target} PRIVATE "-w")
		endif ()
	endforeach()
endfunction()


# Save the current warning settings to an internal variable.
function(push_warnings)
	get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	set(langs C)
	if ("CXX" IN_LIST languages)
		list(APPEND langs CXX)
	endif ()

	foreach(lang ${langs})
		if (CMAKE_${lang}_FLAGS MATCHES ";")
			message(AUTHOR_WARNING "Cannot push warnings for ${lang}, CMAKE_${lang}_FLAGS contains semicolons")
			continue()
		endif ()
		# Add current flags to end of internal list.
		list(APPEND _enable_warnings_internal_${lang}_flags_stack "${CMAKE_${lang}_FLAGS}")
		# Propagate results up to caller's scope.
		set(_enable_warnings_internal_${lang}_flags_stack "${_enable_warnings_internal_${lang}_flags_stack}" PARENT_SCOPE)
	endforeach()
endfunction()


# Restore the current warning settings from an internal variable.
function(pop_warnings)
	get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	set(langs C)
	if ("CXX" IN_LIST languages)
		list(APPEND langs CXX)
	endif ()

	foreach(lang ${langs})
		if (NOT _enable_warnings_internal_${lang}_flags_stack)
			continue()
		endif ()
		# Pop flags off of end of list, overwrite current flags with whatever we popped off.
		list(GET _enable_warnings_internal_${lang}_flags_stack -1 CMAKE_${lang}_FLAGS)
		list(REMOVE_AT _enable_warnings_internal_${lang}_flags_stack -1)
		# Propagate results up to caller's scope.
		set(_enable_warnings_internal_${lang}_flags_stack "${_enable_warnings_internal_${lang}_flags_stack}" PARENT_SCOPE)
		string(STRIP "${CMAKE_${lang}_FLAGS}" CMAKE_${lang}_FLAGS)
		set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}" PARENT_SCOPE)
	endforeach()
endfunction()
