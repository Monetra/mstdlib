cmake_minimum_required(VERSION 3.4.3)

project(mstdlib_thread
	VERSION   ${MSTDLIB_VERSION_STRING}
	LANGUAGES C
)
set(PROJECT_SOVERSION ${MSTDLIB_SOVERSION_STRING})


set(sources
	m_atomic.c
	m_popen.c
	m_thread.c
	m_threadpool.c
	m_thread_attr.c
	m_thread_pipeline.c
	m_thread_rwlock_emu.c
	m_thread_tls.c
)

if (WIN32 OR MINGW)
	list(APPEND sources
		m_thread_win.c
		m_pollemu.c
	)
elseif (HAVE_PTHREAD)
	list(APPEND sources
		m_thread_pthread.c
	)
endif ()

# Android really doesn't like coop threads. For some reason just having coop threads as thread
# model in M_thread_models prevents threading from starting at all. So we're not going to build
# it on Android.
#
# iOS coop uses non-public APIs and Apple rejects app using non-public APIs.
# So we don't build on that one either.
if (NOT ANDROID AND NOT IOS AND NOT IOSSIM)
	list(APPEND sources
		m_thread_coop.c
	)
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

# Add any extra include directories.
target_include_directories(${PROJECT_NAME}
	PUBLIC  "$<INSTALL_INTERFACE:${MSTDLIB_INSTALL_LOCATION_HEADERS}>"
	        "$<BUILD_INTERFACE:${MSTDLIB_TOPLEVEL_DIR}/include>"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link against any dependencies from other modules.
target_link_libraries(${PROJECT_NAME}
	PUBLIC  Mstdlib::base
	PRIVATE Mstdlib::config ${CMAKE_THREAD_LIBS_INIT}
)

# Link against any extra system libraries.
if (WIN32 OR MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE Ws2_32.lib
	)
endif ()

# Versioning on the library.
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION   ${PROJECT_VERSION}
	SOVERSION ${PROJECT_SOVERSION}
	EXPORT_NAME thread
)

# Windows does not normally prefix libraries with "lib", but it seems that CMake on
# windows when using MINGW does, override that behavior.
if (WIN32 AND MINGW)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX        ""
		IMPORT_PREFIX ""
	)
endif ()

convert_mingw_implibs_to_vs(mstdlib_thread)

# Installation rules.
if (MSTDLIB_INSTALL_LIBS)
	install(TARGETS ${PROJECT_NAME} EXPORT Mstdlib ${lib_dest})
endif ()


# Alias target for use from other modules. Name must match the import lib returned by FindMstdlib.
add_library(Mstdlib::thread ALIAS ${PROJECT_NAME})
