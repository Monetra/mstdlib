# ConditionalOption.cmake
#
# Works like option(), except adds extra handling for the given condition.
#
# If this is the first run through and the option variable isn't set, the default value is set to the value
# of the condition. If the condition is false, a warning message is displayed.
#
# If the option variable is already set to false, but this isn't the first run, and the value of the
# condition just changed from false on the last run to true on this one, the option's value is flipped to true.
#
# If the option's value is already set to true, but the condition is false, a fatal warning is displayed and
# configuration is aborted.
#
#
# The purpose of this is to make it easy to create an option that controls building a particular module that
# requires certain third-party dependencies in order to build.
#
# For example:
#   include(ConditionalOption)
#   
#   set(has_deps TRUE)
#   set(reason)
#   find_package(ZLIB)
#   if (NOT TARGET ZLIB::ZLIB)
#     set(has_deps FALSE)
#     set(reason "missing ZLIB::ZLIB, try -DZLIB_DIR=<path>")
#   endif ()
#
#   conditional_option(BUILD_MY_LIB ${has_deps} "Build my_lib" "${reason}")
#
#   if (BUILD_MY_LIB)
#     add_library(my_lib SHARED my_lib.c)
#     target_link_libraries(my_lib PRIVATE ZLIB::ZLIB)
#   endif ()
#
# On the first configure, if you don't explicitly set BUILD_MY_LIB, this will enable building the library if ZLIB
# was found, or disable it with a descriptive message if it wasn't (but will still let configuration continue).
#
# If it's not found on the first configure, but the user then sets "-DZLIB_DIR=..." to the proper path on a second
# configure, then the library will be built.
#
# If the user explicitly sets BUILD_MY_LIB=TRUE when they run cmake, but ZLIB isn't found, this will trigger a
# fatal error message describing the problem.
#

function(conditional_option varname condition option_desc false_reason)
	# If not first run, get value of condition from previous run.
	unset(old_condition)
	if (DEFINED _internal_conditional_option_old_cond_${varname})
		set(old_condition ${_internal_conditional_option_old_cond_${varname}})
	endif ()

	# Save current value of condition for subsequent use on next run.
	set(_internal_conditional_option_old_cond_${varname} ${condition} CACHE INTERNAL "")

	# There are three possible states the build option variable could be in:
	#  - default value (first run, not set yet) (enable if deps found, disable and warn if not)
	#  - explicitly disabled
	#  - explicitly enabled (hard fail if deps not found)

	# Handle default value case (option only changes the value if it's not defined).
	option(${varname} "Build ${option_desc}?" ${condition})

	# Handle explicitly disabled case.
	if (NOT ${varname})
		if (condition AND DEFINED old_condition AND NOT old_condition)
			# If condition is true now, but wasn't on the last run, change explicit disable to explicit enable.
			set(${varname} TRUE CACHE BOOL "Build ${option_desc}?" FORCE)
		else ()
			# Otherwise, return without adding any targets.
			if (false_reason)
				message(STATUS "Building ${option_desc} ... Disabled (${false_reason})")
			else ()
				message(STATUS "Building ${option_desc} ... Disabled")
			endif ()
			return()
		endif ()
	endif ()

	# Handle explicitly enabled, but deps not found case.
	if (${varname} AND NOT condition)
		message(FATAL_ERROR "Building ${option_desc} ... Error (${false_reason})")
	endif ()

	message(STATUS "Building ${option_desc} ... Enabled")
endfunction()

