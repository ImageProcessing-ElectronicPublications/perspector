.SUFFIXES: .in

ROOT ?= ..
include ${ROOT}/config.mk

manpages = ${cmdname}.1

all: ${manpages}

.in:
	awk -v date=`date '+%Y-%m-%d'` 'BEGIN {section="$@"; sub (/.*\./, "", section); manname=toupper("$@"); sub(/\.[^.]+$$/, "", manname); print ".ds appname ${appname}\n.ds authors ${authors}\n.ds cmdname ${cmdname}\n.ds date " date "\n.ds manname " manname "\n.ds section " section "\n.ds version ${version}\n.ds year ${year}\n"}' > $@
	cat $< >> $@

clean:
	rm -f ${manpages}
