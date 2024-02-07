About
=====

mstdlib stands for "M Standard Library".

It was created by Monetra Technologies LLC (fka Main Street Softworks, Inc) for
use in their Monetra payment engine. It is designed to be a safe cross platform
API for functionality that was commonly used in Monetra and other projects. This
library was found to be so useful internally that in 2017 it was decided to
release it to the public as open source. Please see the LICENSE file.

To avoid trademark issues the "m" in mstdlib just stands for the letter M. It
does not stand for Monetra nor any other name starting with M. Though some day we may
become more clever with the meaning of M. (maybe "Mighty")?


Rationale/Use
=============

Mstdlib is designed to provide features not normally found in libc but also
to be used in place of libc. Underneath mstdlib uses the system's libc when it
makes sense. It is not itself a libc implementation. Though it does provide a
number of additional features. Using mstdlib as if it was libc provides benefits
such as NULL safety and memory zeroing. That's not to say you can't use libc
functions directly, but we encourage avoiding it if at all possible.

Mstdlib marks a number of libc functions as deprecated in favor of mstdlibs
versions. Due to features such as NULL safety and secure implementations
mstdlib's functions should be preferred. Using a libc function that has been
implemented in mstdlib will cause a warning to be generated during compilation.
This is to aid in finding libc function (mis)use.

A number of additional functions are provided that are non-standard but often
found in some common libc implementations. This ensures consistency across
multiple platforms.

To aid with reducing coding errors a number of attribute warnings are set (as
supported by the compiler in use). These provide additional hints to the
compiler and cause additional warnings to be generated. For example, checking if
NULL is passed as a parameter when NULL is not a valid input (mstdlib will
prevent a crash in this situation). Or checking if a function's return value is
never set to a variable.

Using mstdlib means only mstdlib.h and mstdlib_*.h files need to be
\#include(d). This removes the need to determine what functions belong to which
header file.

Mstdlib headers should reference the mstdlib directory. For example:

    #include <mstdlib/mstdlib.h>
    #include <mstdlib/mstdlib_thread.h>

Since threading requires the base mstdlib you only need to include
`mstdlib_thread.h`.  All mstdlib functionality will be available because
`mstdlib_thread.h` itself includes `mstdlib.h`. That said, this behavior should
not be relied on. Always include the mstdlib components you intend to use.

Goals
=====

Provide an easy to use C library that is first and foremost secure. It should
provide common functionality to make development easier and faster. It should
work across multiple platforms. There shouldn't be any surprises.

1. Secure
---------

- Make all functions NULL safe.
- Provide helper objects to prevent common vulnerabilities such as buffer
  overflow. For example `M_buf` and `M_parser`.
- Type safety is heavily pushed. Generic implementations are provided for
  creating type safe wrappers. Use of void pointers should be limited.
- M_malloc and M_free are wrappers around the system calls but M_free will zero
  the memory. This is to prevent sensitive data (credit card numbers,
  encryption keys) from being accessible longer than absolutely necessary.
- Data types/formats/parsers were designed to prevent known attack vectors.
- Build with high warning levels and fix any thing reported.
- Use clang's static analyzer so all code is clang clean.
- Consistent naming and design patterns that make an object's use and life
  clear. Such as create and destroy naming.
- Prefer code that is easy to read over clever.

2. Cross platform
-----------------

- All features (when feasible) should work across multiple OSs.
- The same objects and functions should work the same on all OSs.
- Works on:
  - Linux
  - OS X
  - Windows
  - FreeBSD
  - SCO
  - Android
  - iOS
- Tested with multiple compilers and compiler versions.

3. Small
--------

- Don't try to create a library with everything including the kitchen sink.
  This isn't mean to be an all encompassing library like Java or Python's
  standard libraries. (At least this was the original idea but...)
- Features included must be commonly used and useful.
- Use separate modules that are layered in order to reduce the overall size
  of the library if only some features are needed.


Building
========

CMake is the preferred method to build and should be used whenever possible.
However, CMake is not available on all supported platforms. Autotools is also
supported for Unix systems. NMake Makefiles are also available for Windows.
Autotools and NMake Makefiles are provided only as a fallback when CMake cannot
be used. That said, Autotools and NMake Make files can lag behind being updated
and there is no  timeline on how long they will be supported.

The following features are supported by each build system:

Feature                                | CMake | Autotools | NMake Makefiles
:--------------------------------------|:-----:|:---------:|:--------------:
Shared build                           | Y     | Y         | Y
Static build                           | Y     | Y         | N
Disabling building non-base components | Y     | Y         | N
Installation (header and library)      | Y     | Y         | N
Disabling header installation          | Y     | N         | N
Disabling library installation         | Y     | N         | N
Tests                                  | Y     | Y         | N

CMake
-----

    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=<DEBUG|RELEASE> ..
    $ make


Dependencies
------------

### Required

#### C-Ares
The I/O module uses c-ares for DNS resolution as part of its network support.
It is recommended to simply extract the latest c-ares source into the mstdlib
source tree under:

    thirdparty/c-ares

If it exists, it will be built as part of mstdlib and statically linked.

Otherwise, mstdlib will attempt to search for C-Ares on the system. Use
`-Dc_ares_DIR=<path>` if installed to a non-standard location.

#### OpenSSL
The TLS subsystem uses OpenSSL 1.0.1 or higher for the underlying cryptographic
routines.  It is expected to be installed on the system. Pass
`-DOPENSSL_ROOT_DIR=<path>` to cmake if the location is in a non-standard path.

### Optional

#### SQLite
SQLite is used to build the SQLite plugin for the SQL subsystem.  It is
recommended to download the latest SQLite Amalgamation package from sqlite.org
and extract it into the mstdlib source tree under:

    thirdparty/sqlite-amalgamation

If it exists, it will be built as part of mstdlib and statically linked.

Otherwise, mstdlib will attempt to search for SQLite on the system.  Use
`-DSQLITE_DIR=<path>` if installed to a non-standard location.

#### MySQL Client Library/MariaDB Connector C
The MySQL Client Library, or MariaDB Connector C (recommended) are used to build
the MySQL plugin for the SQL subsystem.  Mstdlib will attempt to search for
one of these libraries on the system, use `-DMYSQL_DIR=<path>` if installed to a
non-standard location.

#### PostgreSQL Client Library
The PostgreSQL Client Library is used to build the PostgreSQL plugin for the SQL
subsystem.  v9.2 or higher is required. Mstdlib will attempt to search for the
library on the system, use `-DPOSTGRESQL_DIR=<path>` if installed to a
non-standard location.

#### Oracle Client Library
The Oracle Client Library is used to build the Oracle plugin for the SQL
subsystem.  v11.2 or higher is required.  Mstdlib will attempt to search for the
library on the system, use `-DORACLE_DIR=<path>` if installed to a non-standard
location.

#### ODBC (UnixODBC, iODBC, or Windows ODBC)
The ODBC libraries are used to build the ODBC plugin for the SQL subsystem.
Mstdlib will attempt to search for one of the available options, use
`-DODBC_DIR=<path>` if installed to a non-standard location.

#### DB2
The DB2 libraries are used to build the DB2 plugin for the SQL subsystem.
Mstdlib will attempt to search for the library on the system, use
`-DDB2_DIR=<path>` if installed to a non-standard location.


Building docs
-------------

Requires doxygen is installed and found. Documentation will be build
automatically when the library is built.

Running tests
-------------

Requires libcheck is installed and found.

    $ make check

Logs will be in: build/test/test_name.log.

Debugging tests
---------------

    $ gdb ./bin/test
    (gdb) set environment CK_FORK=no
    (gdb) run

Running tests though Valgrind
-----------------------------

Tests by default will be run though valgrind if it is installed
on the system. Logs with valgrind output will be in:
build/test/test_name-valgrind.log

Build Options
-------------

Option                           | Description                        | Default
---------------------------------|------------------------------------|--------
MSTDLIB_STATIC                   | Build static libs (PIC enabled).   | OFF
MSTDLIB_INSTALL_LOCATION_LIBS    | Alternative install location to use instead of CMAKE_INSTALL_PREFIX. Useful for chain building and installing into a packing sub directory. | N/A
MSTDLIB_INSTALL_LOCATION_HEADERS | Alternative install location to use instead of CMAKE_INSTALL_PREFIX. Useful for chain building and installing into a packing sub directory. | N/A
MSTDLIB_INSTALL_HEADERS          | Install headers.                   | ON
MSTDLIB_INSTALL_LIBS             | Install libraries.                 | ON
MSTDLIB_INSTALL_EXPORTS          | Install CMake export files.        | ON
MSTDLIB_BUILD_TESTS              | Build tests.                       | ON
M_ASAN                           | Build with Address Sanitizer (compiler support required) | OFF
MSTDLIB_USE_VALGRIND             | Run tests with valgrind when running "make check". | ON
MSTDLIB_BUILD_THREAD             | Build thread module.               | ON
MSTDLIB_BUILD_IO                 | Build io module.                   | ON
MSTDLIB_BUILD_TLS                | Build tls module.                  | ON
MSTDLIB_BUILD_LOG                | Build log module.                  | ON
MSTDLIB_BUILD_TEXT               | Build text module.                 | ON
MSTDLIB_BUILD_SQL                | Build sql module.                  | ON
MSTDLIB_BUILD_SQL_MYSQL          | Build MySQL/MariaDB plugin for sql | ON
MSTDLIB_BUILD_SQL_SQLITE         | Build SQLite plugin for SQL        | ON
MSTDLIB_BUILD_SQL_POSTGRESQL     | Build PostgreSQL plugin for SQL    | ON
MSTDLIB_BUILD_SQL_ORACLE         | Build Oracle plugin for SQL        | ON
MSTDLIB_BUILD_SQL_ODBC           | Build ODBC plugin for SQL          | ON
MSTDLIB_BUILD_SQL_DB2            | Build DB2 plugin for SQL           | ON

