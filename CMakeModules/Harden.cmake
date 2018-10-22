# - Check for various cflags and linker supported by the compiler to harden the build.
#
# Good overview of hardening flags for GCC and Visual Studio:
#   https://www.rsaconference.com/writable/presentations/file_upload/asec-f02-writing-secure-software-is-hard-but-at-least-add-mitigations_final.pdf
#

if (_internal_harden_already_run)
	return()
endif ()
set(_internal_harden_already_run TRUE)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

function(_harden_check_compile_flag outvarname flag lang)
	string(MAKE_C_IDENTIFIER "HAVE_${flag}" flagvar)
	if (lang STREQUAL "C")
		check_c_compiler_flag("${flag}" ${flagvar})
	else ()
		string(APPEND flagvar "_${lang}")
		check_cxx_compiler_flag("${flag}" ${flagvar})
	endif ()
	if (${flagvar})
		set(${outvarname} TRUE  PARENT_SCOPE)
	else ()
		set(${outvarname} FALSE PARENT_SCOPE)
	endif ()
endfunction()

function(_harden_check_link_flag outvarname flag)
	set(CMAKE_REQUIRED_LIBRARIES ${flag})
	string(MAKE_C_IDENTIFIER "HAVE_${flag}" flagvar)
	check_c_compiler_flag("" ${flagvar})
	if (${flagvar})
		set(${outvarname} TRUE  PARENT_SCOPE)
	else ()
		set(${outvarname} FALSE PARENT_SCOPE)
	endif ()
endfunction()


set(_compile_flags)       # Flags to apply when compiling C/C++ code.
set(_link_flags)          # Flags to apply to all links.
set(_link_flags_exe)      # Flags to apply when linking executables.
set(_link_flags_shared)   # Flags to apply when linking shared libraries.
set(_link_flags_implicit) # Extra system link flags added at end of line for all targets.


# On all platforms, build everything as position independent code (if possible), to allow ASLR.
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)


if (MSVC)
	# Visual Studio
	set(_compile_flags
		"-gs"           # (on by default >= VS2005) enable stack buffer overflow protection (similiar to -fstack-protector)
	)
	set(_link_flags
		#"-safeseh"     # force safe exception handlers only on x86 (doesn't apply to x64 or ARM)
		"-nxcompat"     # (on by default >= VS2008 SP1) opt into Data Execution Prevention (DEP)
		#"-dynamicbase" # (on by default >= VS2010) enable ASLR (make executables relocatable)
	)
else ()
	# Anything that's not Visual Studio (GCC, Clang, AppleClang, MinGW, etc.)

	#list(APPEND _compile_flags "-fvisibility=hidden")

	# Versions of CMake before 3.14 have a bug where it doesn't add "-pie" to executable link lines when
	# you enable position independent code (only adds "-fPIE", which isn't enough).
	#
	# See: https://gitlab.kitware.com/cmake/cmake/issues/14983
	# TODO: remove this once minimum supported CMake version is >= 3.14.
	if (CMAKE_VERSION VERSION_LESS 3.14)
		if (UNIX AND NOT (APPLE OR ANDROID))
			list(APPEND _link_flags_exe -pie)
		endif ()
	else ()
		cmake_policy(SET CMP0083 NEW) # add -pie flag when position independent code is set.
	endif ()

	# Enable stack protector (similiar to Visual Studio's /GS flag). Adds extra guards against stack buffer overflow.
	# The "strong" stack protector is the current best option (see https://lwn.net/Articles/584225/). If we're on
	# an older compiler that doesn't support this option, fall back to basic stack protector instead.
	#
	# Stack protector on AIX is buggy (our installation of GCC on AIX seems to be missing the libssp library).
	#
	# TODO: investigate whether we should try setting -D_FORTIFY_SOURCE, too (though it might not be worth it).
	#
	if (NOT CMAKE_SYSTEM_NAME MATCHES "AIX")
		_harden_check_compile_flag(has_flag "-fstack-protector-strong" "C")
		if (has_flag)
			list(APPEND _compile_flags "-fstack-protector-strong")
		else ()
			# If compiler doesn't support the newer "strong" stack protector option, try the older basic version.
			_harden_check_compile_flag(has_flag "-fstack-protector" "C")
			if (has_flag)
				list(APPEND _compile_flags "-fstack-protector")
			endif ()
		endif ()

		if ((MINGW OR CMAKE_SYSTEM_NAME MATCHES "SunOS") AND has_flag)
			# MinGW/Solaris require you to explicitly link to libssp in some cases when stack
			# protector is enabled.
			list(APPEND _link_flags_implicit "-lssp")
		endif ()
	endif ()

	list(APPEND _link_flags
		# Allow undefined symbols in shared libraries (usually the default, anyway).
		"-Wl,--allow-shlib-undefined"
		# Prune out shared libraries that we link against, but don't actually use.
		"-Wl,--as-needed"
		# Ensure that stack is marked non-executable (same thing as DEP in Visual Studio)
		"-Wl,-z,noexecstack"
		# Enable full RELRO (read-only relocations) - hardens ELF data sections and the GOT (global offsets table)
		# http://tk-blog.blogspot.com/2009/02/relro-not-so-well-known-memory.html
		"-Wl,-z,relro,-z,now"
	)

	if (MINGW)
		# Mingw-specific flags
		#
		# Note that ASLR support is seriously wonky on mingw and mingw-w64. Different
		# versions require different sets of hacks to work right, can't just set the
		# standard "-pie/--pic-executable" GCC flag and be done with it.

		list(APPEND _link_flags
			# enable ASLR (make executables relocatable)
			"-Wl,--dynamicbase"
			# opt into Data Execution Prevention (DEP) when running on Vista or newer.
			"-Wl,--nxcompat"
		)

		list(APPEND _link_flags_shared
			# add hacks to allow DATA imports from a DLL with a non-zero offset.
			"-Wl,--enable-runtime-pseudo-reloc"
			# disable automatic image base calculation - don't need it, because we're using ASLR to rebase everything
			# anyway (see https://lists.ffmpeg.org/pipermail/ffmpeg-cvslog/2015-September/094018.html)
			"-Wl,--disable-auto-image-base"
		)

		list(APPEND _link_flags_exe
			# long name for -pie flag. For some reason, need to specify this AND dynamicbase, even though
			# this flag is really only meant for ELF platforms (not DLL). But MinGW leaves off the relocation
			# information unless this flag is provided.
			#
			# It's some kind of weird bug that's been around since 2013, apparently:
			# https://sourceforge.net/p/mingw-w64/mailman/message/31035280/
			"-Wl,--pic-executable"
		)

		if (CMAKE_SIZEOF_VOID_P EQUAL "8")
			# Flags for MinGW with 64bit output

			list(APPEND _link_flags
				# Enable HEASLR (High Entropy ASLR): allows 64bit address space for ASLR relocations
				"-Wl,--high-entropy-va"
			)

			list(APPEND _link_flags_exe
				# MinGW's LD forgets the entry point when used with pic-executable, fix it.
				# See here: https://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=91b668a
				"-Wl,-e,mainCRTStartup"
			)

			# set image base > 4GB for extra entropy when using HEASLR
			# See here: https://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=a58c22d
			list(APPEND _link_flags_exe
				"-Wl,--image-base,0x140000000"
			)
			list(APPEND _link_flags_shared
				"-Wl,--image-base,0x180000000"
			)
		else ()
			# Flags for MinGW with 32bit output

			list(APPEND _link_flags
				# Allow ASLR to use 4GB instead of 2GB for relocations when a 32bit app is running on a 64bit OS
				#
				# This is mingw's version of the -LARGEADDRESSAWARE option in Visual Studio's linker.
				"-Wl,--large-address-aware"
			)

			list(APPEND _link_flags_exe
				# MinGW's LD forgets the entry point when used with pic-executable, fix it.
				# See here: https://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=91b668a
				"-Wl,-e,_mainCRTStartup"
			)
		endif ()
	endif ()
endif ()


# Check and set compiler flags.
get_property(_enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
foreach(flag ${_compile_flags})
	_harden_check_compile_flag(has_flag "${flag}" "C")
	if (has_flag)
		string(APPEND CMAKE_C_FLAGS " ${flag}")
	endif ()

	if ("CXX" IN_LIST _enabled_languages)
		# If C++ is enabled as a language, add the compile flags to CXX flags too.
		_harden_check_compile_flag(has_flag "${flag}" "CXX")
		if (has_flag)
			string(APPEND CMAKE_CXX_FLAGS " ${flag}")
		endif ()
	endif ()
endforeach()

# Check and set linker flags.
foreach(flag ${_link_flags})
	_harden_check_link_flag(has_flag "${flag}")
	if (has_flag)
		string(APPEND CMAKE_EXE_LINKER_FLAGS    " ${flag}")
		string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${flag}")
		string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${flag}")
	endif ()
endforeach ()

# Check and set exe only linker flags.
foreach(flag ${_link_flags_exe})
	_harden_check_link_flag(has_flag "${flag}")
	if (has_flag)
		string(APPEND CMAKE_EXE_LINKER_FLAGS " ${flag}")
	endif ()
endforeach ()

# Check and set shared only linker flags.
foreach (flag ${_link_flags_shared})
	_harden_check_link_flag(has_flag "${flag}")
	if (has_flag)
		string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${flag}")
		string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${flag}")
	endif ()
endforeach ()

# Check and add to implicit system linker flags.
foreach (flag ${_link_flags_implicit})
	_harden_check_link_flag(has_flag "${flag}")
	if (has_flag)
		list(APPEND CMAKE_C_IMPLICIT_LINK_LIBRARIES "${flag}")
		if ("CXX" IN_LIST _enabled_languages)
			list(APPEND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "${flag}")
		endif ()
	endif ()
endforeach ()
