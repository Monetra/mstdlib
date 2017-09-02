ACLOCAL_AMFLAGS = -I m4 --install

# headers that go in the distribution, but are not installed to @prefix@/include
noinst_HEADERS = config.h m_config.h config.w32.h

SUBDIRS = @BUILD_SUBDIRS@

noinst_SUBDIRS = thirdparty/c-ares

install:
	echo $(SUBDIRS)
	for mydir in $(SUBDIRS) ; do \
		do_install=1 ; \
		for noinstdir in $(noinst_SUBDIRS) ; do \
			if [ "x$${noinstdir}" = "x$${mydir}" ] ; then \
				do_install=0 ; \
				break ; \
			fi \
		done ; \
		if [ "$${do_install}" = "1" ] ; then \
			(cd $${mydir} && ${MAKE} install) \
		fi \
	done

include Makefile.am.doxygen
