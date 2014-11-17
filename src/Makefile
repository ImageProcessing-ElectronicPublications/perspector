ROOT ?= ..
include ${ROOT}/config.mk

CPPFLAGS += -DAUTHORS="${authors}" -DVERSION=${version} -DYEAR=${year}
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -DHAVE_INLINE
CFLAGS += `pkg-config --cflags gtk+-3.0`
LDLIBS += `pkg-config --libs gtk+-3.0`
LDLIBS += -lgsl -lgslcblas
LDLIBS += -lm

## The recipe is the implicit rule of GNU Make. Unfortunately non-GNU Make are
## not so smart at guessing the right recipe so we need to add it here.
${cmdname}: gui.o ${cmdname}.o
	${CC} ${LDFLAGS} ${TARGET_ARCH} gui.o ${cmdname}.o $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: debug
debug:
	CFLAGS+="-g3 -O0 -DDEBUG=9" ${MAKE}

clean:
	rm -f ${cmdname} *.d ${cmdname}.o gui.o

## Generate prerequisites automatically. GNU Make only.
## The 'awk' part is used to add the .d file itself to the target, so that it
## gets updated on changes. The -MQ option is to add full path to object files
## in subfolders. (-MM strips parent directories.)
%.d: %.c
	${CC} -MM -MQ ${<:.c=.o} ${CPPFLAGS} $< | awk -v stem=$* -v target=$@ '{gsub (stem ".o[ :]*", stem ".o " target " : ")} 1' > $@

sources = $(wildcard *.c)
deps = ${sources:.c=.d}
-include ${deps}
