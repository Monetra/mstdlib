# Functions for keeping track of set cache variables and clearing them.
#
# get_cachelog_var
#
# add_to_cachelog
#
# unset_cachelog_entries
#
# reset_cachelog
#


# Get name of variable that cachelog will be stored in for this file.
#
# Name the cachelog variable after the Find* file that called this function.
# This prevents collisions if multiple find modules use these functions.
function(get_cachelog_var _cachelog_var_outname)
	get_filename_component(_log_var "${CMAKE_CURRENT_LIST_FILE}" NAME)
	string(MAKE_C_IDENTIFIER "${_log_var}" _log_var)
	set(${_cachelog_var_outname} "${_log_var}_cachelog" PARENT_SCOPE)
endfunction()


# Add the given cache variable name to the cache log, and mark it as advanced.
function(add_to_cachelog _name)
	get_cachelog_var(_log_var)
	set(${_log_var} ${${_log_var}} ${_name} CACHE INTERNAL "" FORCE)
	mark_as_advanced(FORCE ${_name})
endfunction()


# Unset all cache variables that are listed in the cache log.
function(unset_cachelog_entries)
	get_cachelog_var(_log_var)
	foreach (_cachevar ${${_log_var}})
		unset(${_cachevar} CACHE)
	endforeach ()
endfunction()


# Remove all cache variable names from the cache log.
function(reset_cachelog)
	get_cachelog_var(_log_var)
	set(${_log_var} "" CACHE INTERNAL "" FORCE)
endfunction()
