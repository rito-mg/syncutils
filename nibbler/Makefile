PACKAGE=nibbler
VERSION=1.2
STRIPFLAG=#-s
STATIC=#-static
LDFLAGS=$(STATIC)
EXTRA_CFLAGS=-D_REENTRANT -O2 -D_FILE_OFFSET_BITS=64 -march=native

all: nibbler

nibbler: getopt.c getopt.h nibbler.c md5.c md5.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) getopt.c nibbler.c md5.c -o nibbler $(LDFLAGS) $(LIBS) $(STRIPFLAG)
	-@/bin/ls -ld nibbler

install:
	cp -p nibbler /usr/local/bin/nibbler
	-@chmod a+rX /usr/local/bin/nibbler
	-@/bin/ls -ld /usr/local/bin/nibbler

clean:
	/bin/rm -f *.o nibbler core

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
