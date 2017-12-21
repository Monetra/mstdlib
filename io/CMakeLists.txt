cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_io
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})

# Find and use C-Ares.
if (IS_DIRECTORY "${MSTDLIB_TOPLEVEL_DIR}/thirdparty/c-ares" AND NOT TARGET c-ares::cares)
	# Turn off warnings when compiling c-ares (see EnableWarnings.cmake).
	push_warnings()
	remove_all_warnings()
	# Override the following settings in the subdirectory, and hide them from the user.
	set(CARES_STATIC     TRUE  CACHE INTERNAL "")
	set(CARES_SHARED     FALSE CACHE INTERNAL "")
	set(CARES_INSTALL    FALSE CACHE INTERNAL "")
	set(CARES_STATIC_PIC TRUE  CACHE INTERNAL "")
	add_subdirectory("${MSTDLIB_TOPLEVEL_DIR}/thirdparty/c-ares" "${PROJECT_BINARY_DIR}/c-ares")
	# Restore warnings for rest of this file.
	pop_warnings()
else ()
	find_package(c-ares REQUIRED)
endif ()


set(sources
	m_dns.c
	m_io.c
	m_io_block.c
	m_io_bwshaping.c
	m_io_loopback.c
	m_io_net.c
	m_io_netdns.c
	m_io_serial.c
	m_io_trace.c
	m_io_hid.c
	m_io_bluetooth.c
	m_event.c
	m_event_timer.c
	m_event_trigger.c
)

if (WIN32 OR MINGW)
	list(APPEND sources
		m_event_win32.c
		m_io_osevent_win32.c
		m_io_pipe_win32.c
		m_io_serial_win32.c
		m_io_win32_common.c
		m_io_w32overlap.c
		m_io_w32overlap_busyemu.c
		m_io_hid_win32.c
	)
else ()
	list(APPEND sources
		m_io_pipe_posix.c
		m_io_posix_common.c
		m_io_serial_posix.c
		m_event_poll.c
		m_io_osevent_pipe.c
	)
	if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
		list(APPEND sources
			m_io_hid_linux.c
		)
	elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT IOS AND NOT IOSSIM)
		list(APPEND sources
			m_io_mac_common.c
			m_io_bluetooth_mac.m
			m_io_bluetooth_mac_rfcomm.m
			m_io_hid_mac.c
			m_io_mac_runlooper.c
		)
	else ()
		list(APPEND sources
			m_io_hid_notimpl.c
		)
	endif ()
endif ()

if (ANDROID)
	list(APPEND sources m_io_jni.c)
endif ()

if (ANDROID)
	list(APPEND sources m_io_bluetooth_android.c)
elseif (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin" OR IOS OR IOSSIM)
	list(APPEND sources m_io_bluetooth_notimpl.c)
endif ()

if (IOS OR IOSSIM)
	list(APPEND sources m_io_mfi_ea.m m_io_mfi.m)
else ()
	list(APPEND sources m_io_mfi_notimpl.c)
endif ()

if (HAVE_KQUEUE)
	list(APPEND sources m_event_kqueue.c)
elseif (HAVE_EPOLL)
	list(APPEND sources m_event_epoll.c)
endif ()


# Build the library.
if (MSTDLIB_STATIC)
	add_library(${PROJECT_NAME} STATIC
		${sources}
	)
	target_compile_definitions(${PROJECT_NAME}
		PUBLIC MSTDLIB_STATIC
	)
	if (${MSTDLIB_STATIC_PIC})
		set_target_properties(${PROJECT_NAME} PROPERTIES
			POSITION_INDEPENDENT_CODE TRUE
		)
	endif ()
else ()
	add_library(${PROJECT_NAME} SHARED
		${sources}
	)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		POSITION_INDEPENDENT_CODE TRUE
	)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	target_compile_options(${PROJECT_NAME} PUBLIC "-fobjc-arc")
endif ()


# Set include directories.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
# If Windows, we need the DDK
if (WIN32)
	find_package(WinDDK REQUIRED)
	target_include_directories(${PROJECT_NAME}
		PRIVATE "${WINDDK_INCLUDE_DIRS}"
	)
endif ()


# Link dependencies on other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::thread
	PRIVATE Mstdlib::config c-ares::cares
)

# Link extra system libraries.
if (WIN32 OR MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE Ws2_32.lib winmm.lib Setupapi.lib "${WINDDK_LIBRARIES}"
	)
endif ()

if (ANDROID)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE log
	)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	if (NOT IOS AND NOT IOSSIM)
		find_library(IOKIT NAMES IOKit)
		target_link_libraries(${PROJECT_NAME}
			PUBLIC "${IOKIT}"
		)
		find_library(IOBUETOOTH NAMES IOBluetooth)
		target_link_libraries(${PROJECT_NAME}
			PUBLIC "${IOBUETOOTH}"
		)
		find_library(FOUNDATION NAMES Foundation)
		target_link_libraries(${PROJECT_NAME}
			PUBLIC "${FOUNDATION}"
		)
	endif ()
	find_library(COREFOUNDATION NAMES CoreFoundation)
	target_link_libraries(${PROJECT_NAME}
		PUBLIC "${COREFOUNDATION}"
	)
endif ()


# Versioning on the library.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION   ${PROJECT_VERSION}
	SOVERSION ${PROJECT_SOVERSION}
	EXPORT_NAME io
)


# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()


# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::io ALIAS ${PROJECT_NAME})
