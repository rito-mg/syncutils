PACKAGE=bigfilemaker
VERSION=1.3
STRIPFLAG=#-s
STATIC=#-static
LDFLAGS=$(STATIC)
EXTRA_CFLAGS=-D_REENTRANT -O2 -D_FILE_OFFSET_BITS=64 -march=native

all: bigfilemaker

bigfilemaker: drand48.c getopt.c getopt.h main.c md5.c md5.h rand48.h Strncat.c Strncpy.c Strn.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) drand48.c getopt.c main.c md5.c Strncat.c Strncpy.c -o bigfilemaker $(LDFLAGS) $(LIBS) $(STRIPFLAG)
	-@/bin/ls -ld bigfilemaker

install:
	cp -p bigfilemaker /usr/local/bin/bigfilemaker
	-@chmod a+rX /usr/local/bin/bigfilemaker
	-@/bin/ls -ld /usr/local/bin/bigfilemaker

taroldway:
	tar -C .. --exclude='*.o' --exclude='*.tar.gz' --exclude='*.swp'  --exclude=bigfilemaker/bigfilemaker -c -v -z -f bigfilemaker.tar.gz bigfilemaker

clean:
	/bin/rm -f *.o bigfilemaker core

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
