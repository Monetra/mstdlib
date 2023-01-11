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


# Some of these are going to be stubs with
# OS specific implementations added later.
# E.g. HID, BLE, Bluetooth, serial.
set(sources
	# No OS specific implementations
	m_io.c
	m_io_block.c
	m_io_bwshaping.c
	m_io_loopback.c
	net/m_dns.c
	net/m_io_net.c
	net/m_io_netdns.c
	net/m_io_net_iface_ips.c
	m_io_meta.c
	m_io_process.c
	m_io_proxy_protocol.c
	m_io_trace.c

	# Stubs
	m_io_ble.c
	m_io_ble_rdata.c
	m_io_ble_wcomplete.c
	m_io_bluetooth.c
	m_io_hid.c
	m_io_serial.c

	# Event
	m_event.c
	m_event_timer.c
	m_event_trigger.c
)

# Serial, HID
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
			m_io_hid_mac.c
			m_io_mac_runlooper.c
		)
	elseif (ANDROID)
		list(APPEND sources
			m_io_hid_android.c
		)
	else ()
		list(APPEND sources
			m_io_hid_notimpl.c
		)
	endif ()
endif ()

if (ANDROID)
	list(APPEND sources m_io_jni.c)

	# Replacement for getifaddrs() for Android API < 24
	if (CMAKE_SYSTEM_VERSION LESS 24)
		list(APPEND sources net/android_ifaddrs/ifaddrs.c)
	fi
endif ()

# Bluetooth LE
set(supports_ble FALSE)
if (IOS OR IOSSIM)
	set(supports_ble TRUE)
elseif (APPLE)
	# MacOS only supports our use case for BLE in version 10.13 or later.
	if (CMAKE_OSX_DEPLOYMENT_TARGET)
		if (NOT CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.13")
			set(supports_ble TRUE)
		endif ()
	else ()
		if (NOT CMAKE_SYSTEM_VERSION VERSION_LESS "17.0.0") # MacOS 10.13 --> v17.0.0
			set(supports_ble TRUE)
		endif ()
	endif ()
endif ()

if (supports_ble)
	list(APPEND sources
		m_io_ble_mac_cache.m
		m_io_ble_mac_manager.m
		m_io_ble_mac.m
	)
else ()
	list(APPEND sources m_io_ble_notimpl.c)
endif ()

# Bluetooth Classic
if (ANDROID)
	list(APPEND sources m_io_bluetooth_android.c)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT IOS AND NOT IOSSIM)
	list(APPEND sources
		m_io_bluetooth_mac.m
		m_io_bluetooth_mac_rfcomm.m
	)
else ()
	list(APPEND sources m_io_bluetooth_notimpl.c)
endif ()

# MFi
if (IOS OR IOSSIM)
	list(APPEND sources m_io_mfi_ea.m m_io_mfi.m)
else ()
	list(APPEND sources m_io_mfi_notimpl.c)
endif ()

# Queue
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
else ()
	add_library(${PROJECT_NAME} SHARED
		${sources}
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
	        Mstdlib::text
	PRIVATE Mstdlib::config c-ares::cares ${CMAKE_THREAD_LIBS_INIT}
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
	if (IOS OR IOSSIM)
		find_library(EXTERNALACCESSORY NAMES ExternalAccessory)
		target_link_libraries(${PROJECT_NAME}
			PUBLIC "${EXTERNALACCESSORY}"
		)
	else ()
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
	find_library(COREBLUETOOTH NAMES CoreBluetooth)
	target_link_libraries(${PROJECT_NAME}
		PUBLIC "${COREBLUETOOTH}"
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

convert_mingw_implibs_to_vs(mstdlib_io)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::io ALIAS ${PROJECT_NAME})
