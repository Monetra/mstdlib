# Store version info in header (uses MSTDLIB_VERSION_MAJOR, *_MINOR, *_PATCH, *_STRING)
configure_file(mstdlib/mstdlib_version.h.cmake mstdlib/mstdlib_version.h @ONLY)

# Installation rules.
if (MSTDLIB_INSTALL_HEADERS)
	set(dest "${MSTDLIB_INSTALL_LOCATION_HEADERS}/mstdlib")

	# Always install base
	install(
		FILES       mstdlib/mstdlib.h
		            ${CMAKE_CURRENT_BINARY_DIR}/mstdlib/mstdlib_version.h
	    DESTINATION "${dest}"
	)
	install(DIRECTORY mstdlib/base/ DESTINATION "${dest}/base" FILES_MATCHING PATTERN "*.h")

	# Install other components based on if they are enabled or not.
	if (MSTDLIB_BUILD_BACKTRACE)
		install(FILES     mstdlib/mstdlib_backtrace.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/backtrace/          DESTINATION "${dest}/backtrace" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_FORMATS)
		install(FILES     mstdlib/mstdlib_formats.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/formats/          DESTINATION "${dest}/formats" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_THREAD)
		install(FILES     mstdlib/mstdlib_thread.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/thread/          DESTINATION "${dest}/thread" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_IO)
		install(FILES     mstdlib/mstdlib_io.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/io/          DESTINATION "${dest}/io" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_TLS)
		install(FILES     mstdlib/mstdlib_tls.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/tls/          DESTINATION "${dest}/tls" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_NET)
		install(FILES     mstdlib/mstdlib_net.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/net/          DESTINATION "${dest}/net" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_LOG)
		install(FILES     mstdlib/mstdlib_log.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/log/          DESTINATION "${dest}/log" FILES_MATCHING PATTERN "*.h")
	endif ()

	if (MSTDLIB_BUILD_SQL)
		install(FILES     mstdlib/mstdlib_sql.h DESTINATION "${dest}")
		install(DIRECTORY mstdlib/sql/          DESTINATION "${dest}/sql" FILES_MATCHING PATTERN "*.h")
	endif ()
endif ()
