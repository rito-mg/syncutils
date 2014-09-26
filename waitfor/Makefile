PACKAGE=waitfor
VERSION=1.0
STRIPFLAG=#-s
STATIC=#-static
LDFLAGS=$(STATIC)
# EXTRA_CFLAGS=-D_REENTRANT -O2 -D_FILE_OFFSET_BITS=64 -march=native
EXTRA_CFLAGS=-D_REENTRANT -g -D_FILE_OFFSET_BITS=64 -march=native

all: waitfor

waitfor: getopt.c getopt.h waitfor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) getopt.c waitfor.c -o waitfor $(LDFLAGS) $(LIBS) $(STRIPFLAG)
	-@/bin/ls -ld waitfor

install:
	cp -p waitfor /usr/local/bin/waitfor
	-@chmod a+rX /usr/local/bin/waitfor
	-@/bin/ls -ld /usr/local/bin/waitfor

clean:
	/bin/rm -f *.o waitfor core

tar:
	cat /dev/null > "$(PACKAGE)-$(VERSION).tar.gz"	# must exist to prevent tar error
	tar \
		--transform 's!^\.!'"$(PACKAGE)-$(VERSION)"'!' \
		--exclude-backups \
		--exclude='*.o' \
		--exclude='bin' \
		--exclude="$(PACKAGE)" \
		--exclude='core' \
		--exclude='.dep*' \
		--exclude='*cache*' \
		--exclude='*~' \
		--exclude='./.???*' \
		--exclude='config.h' \
		--exclude='stamp-h1' \
		--exclude='config.status' \
		--exclude='config.log' \
		--exclude='*.tar*' \
		--exclude='*.tgz' \
		-z -c -v \
		-f "$(PACKAGE)-$(VERSION).tar.gz" \
		.
	-@touch -r `/bin/ls -1 -t *.c | sed -n 1,1p` "$(PACKAGE)-$(VERSION).tar.gz"
	-@/bin/ls -ld "$(PACKAGE)-$(VERSION).tar.gz"
	@test -s "$(PACKAGE)-$(VERSION).tar.gz"
