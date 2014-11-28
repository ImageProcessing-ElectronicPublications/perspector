ROOT = .
include ${ROOT}/config.mk

################################################################################
## Build.

.PHONY: all
all: app doc

.PHONY: app
app:
	${MAKE} -C ${srcdir}

.PHONY: doc
doc:
	${MAKE} -C ${docsrcdir}

.PHONY: debug
debug:
	CFLAGS+="-g3 -O0 -DDEBUG=9" ${MAKE}

.PHONY: test
test:
	${MAKE} -C ${testdir}

.PHONY: clean
clean:
	${MAKE} -C ${srcdir} clean
	${MAKE} -C ${docsrcdir} clean
	${MAKE} -C ${testdir} clean

################################################################################
## Install / Uninstall.

INSTALL ?= install
INSTALL_DATA ?= ${INSTALL} -m644
INSTALL_DIR ?= ${INSTALL} -d

prefix ?= /usr/local
exec_prefix ?= ${prefix}
datarootdir ?= ${prefix}/share

bindir ?= ${exec_prefix}/bin
datadir ?= ${datarootdir}
docdir ?= ${datarootdir}/doc
includedir ?= ${prefix}/include
infodir ?= ${datarootdir}/info
libdir ?= ${exec_prefix}/lib
libexecdir ?= ${exec_prefix}/libexecdir
licensedir ?= ${datarootdir}/licenses
localedir ?= ${datarootdir}/locale
localstatedir ?= ${prefix}/var
mandir ?= ${datarootdir}/man
runstatedir ?= ${prefix}/run
sbindir ?= ${exec_prefix}/sbin
sharedstatedir ?= ${prefix}/com
sysconfdir ?= ${perfix}/etc

.PHONY: install
install:
	${MAKE}
	${INSTALL_DIR} ${DESTDIR}${bindir}
	${INSTALL} ${srcdir}/${cmdname} ${DESTDIR}${bindir}/${cmdname}
	${INSTALL_DIR} ${DESTDIR}${mandir}/man1
	${INSTALL_DATA} ${docsrcdir}/${cmdname}.1 ${DESTDIR}${mandir}/man1/${cmdname}.1
	${INSTALL_DIR}  ${DESTDIR}${licensedir}/${cmdname}
	${INSTALL_DATA} LICENSE ${DESTDIR}${licensedir}/${cmdname}/LICENSE

.PHONY: uninstall
uninstall:
	-rm -f ${DESTDIR}${bindir}/${cmdname}
	-rmdir -p ${DESTDIR}${bindir}
	-rm -f ${DESTDIR}${mandir}/${cmdname}.${mansection}.gz
	-rmdir -p ${DESTDIR}${mandir}
	-rm -f ${DESTDIR}${licensedir}/${cmdname}/LICENSE
	-rmdir -p ${DESTDIR}${licensedir}/${cmdname}
