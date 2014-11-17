ROOT ?= ..
include ${ROOT}/config.mk

CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -DHAVE_INLINE
CPPFLAGS += -I${ROOT}/${srcdir}
CFLAGS += -g3 -O0 -DDEBUG=9
LDLIBS += -lgsl -lgslcblas
LDLIBS += -lm

tests: ${ROOT}/${srcdir}/${cmdname}.o tests.o
	${CC} ${LDFLAGS} ${TARGET_ARCH} ${ROOT}/${srcdir}/${cmdname}.o tests.o $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f tests tests.d tests.o

# include ${ROOT}/autodeps.mk
