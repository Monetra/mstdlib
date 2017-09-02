#!/bin/sh
if [ -d "thirdparty/c-ares" ] ; then
	( cd thirdparty/c-ares && ./buildconf ) || exit 1
fi
autoreconf -iv --no-recursive || exit 1
