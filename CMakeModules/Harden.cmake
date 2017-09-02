# - Check for various cflags and linker supported by the compiler to harden the build.

if (_internal_harden_already_run)
	return()
endif ()
set(_internal_harden_already_run TRUE)

include(CheckCCompilerFlag)

# Save these.
set(MYCMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})

#SET (CMAKE_SKIP_RPATH ON)

set(C_FLAGS
	#"-fvisibility=hidden"
)
set(LINKER_FLAGS
	#"-Wl,--no-undefined"
	"-Wl,--allow-shlib-undefined"
	"-Wl,--as-needed"
	"-Wl,-z,noexecstack"
	"-Wl,-z,relro,-z,now"
)

set(LINKER_FLAGS_EXE_ONLY)
if (WIN32 AND MSVC)
	set(WIN32_C_FLAGS
		"-gs"
	)
	set(WIN32_LINKER_FLAGS
		#"-safeseh"
		"-nxcompat"
		#"-dynamicbase"
	)
endif ()

if (MINGW)
	# Common for DLL and EXE
	set(LINKER_FLAGS
		"${LINKER_FLAGS}"
		# Needed for ASLR
		"-Wl,--dynamicbase"
		# Non Executable Stack/DEP
		"-Wl,--nxcompat"
	)

	# Common for DLL only
	set(LINKER_FLAGS_SHARED_ONLY
		"${LINKER_FLAGS_SHARED_ONLY}"
		"-Wl,--enable-runtime-pseudo-reloc"
		"-Wl,--disable-auto-image-base"
	)

	# Common for EXE only
	set(LINKER_FLAGS_EXE_ONLY
		"${LINKER_FLAGS_EXE_ONLY}"
		# Needed for ASLR
		"-Wl,--pic-executable"
	)

	if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		# 64bit for DLL and EXE
		set(LINKER_FLAGS
			"${LINKER_FLAGS}"
			# Allow 64bit address space for ASLR relocations
			"-Wl,--high-entropy-va"
		)					

		# 64bit EXE only				
		set(LINKER_FLAGS_EXE_ONLY
			"${LINKER_FLAGS_EXE_ONLY}"
			# LD forgets the entry point when used with pic-executable, fix it
			"-Wl,-e,mainCRTStartup"
			# image base > 4GB for HEASLR (needed with high-entropy-va?)
			"-Wl,--image-base,0x140000000"
		)

		# 64bit DLL only
		set(LINKER_FLAGS_SHARED_ONLY
			"${LINKER_FLAGS_SHARED_ONLY}"
			# image base > 4GB for HEASLR (needed with high-entropy-va?)
			"-Wl,--image-base,0x180000000"
		)
	else ()
		# 32bit for DLL and EXE
		set(LINKER_FLAGS
			"${LINKER_FLAGS}"
			# Allow ASLR to use 4GB instead of 2GB for relocations when
			# a 32bit app is running on a 64bit OS
			"-Wl,--large-address-aware"
		)

		# 32bit EXE only
		set(LINKER_FLAGS_EXE_ONLY
			"${LINKER_FLAGS_EXE_ONLY}"
			# LD forgets the entry point when used with pic-executable, fix it
			"-Wl,-e,_mainCRTStartup"
		)
	endif ()
endif ()

# Check and set compiler flags.
foreach (flag ${C_FLAGS})
	CHECK_C_COMPILER_FLAG(${flag} HAVE_${flag})
	if (HAVE_${flag})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif ()
endforeach ()

# Check and set linker flags.
foreach (flag ${LINKER_FLAGS})
	set(CMAKE_REQUIRED_FLAGS ${flag})
	CHECK_C_COMPILER_FLAG ("" HAVE_${flag})
	if(HAVE_${flag})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
	endif ()
endforeach ()

# Check and set exe only linker flags.
foreach (flag ${LINKER_FLAGS_EXE_ONLY})
	set(CMAKE_REQUIRED_FLAGS ${flag})
	CHECK_C_COMPILER_FLAG ("" HAVE_${flag})
	if(HAVE_${flag})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
	endif ()
endforeach ()

# Check and set shared only linker flags.
foreach (flag ${LINKER_FLAGS_SHARED_ONLY})
	set(CMAKE_REQUIRED_FLAGS ${flag})
	CHECK_C_COMPILER_FLAG ("" HAVE_${flag})
	if (HAVE_${flag})
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
	endif ()
endforeach ()

# Windows specific flags. The variable won't be set if not on Windows.
foreach (flag ${WIN32_C_FLAGS})
	CHECK_C_COMPILER_FLAG(${flag} HAVE_${flag})
	if (HAVE_${flag})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif ()
endforeach ()

foreach (flag ${WIN32_LINKER_FLAGS})
	set(CMAKE_REQUIRED_FLAGS ${flag})
	CHECK_C_COMPILER_FLAG ("" HAVE_${flag})
	if (HAVE_${flag})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
	endif ()
endforeach ()

set(CMAKE_REQUIRED_FLAGS ${MYCMAKE_REQUIRED_FLAGS})
