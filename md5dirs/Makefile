PACKAGE=md5dirs
VERSION=1.2
STRIPFLAG=#-s
STATIC=#-static
LDFLAGS=$(STATIC)

all: md5dirs

md5dirs: getopt.c getopt.h main.c md5.c md5.h resource.h
	$(CC) $(CPPFLAGS) $(CFLAGS) getopt.c main.c md5.c Strncat.c Strncpy.c -o md5dirs $(LDFLAGS) $(LIBS) $(STRIPFLAG)
	-@/bin/ls -ld md5dirs

clean:
	/bin/rm -f *.o md5dirs core

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
		--exclude='md5dirs' \
		--exclude='md5dirs.exe' \
		--exclude='*.log' \
		--exclude='*.aps' \
		-z -c -v \
		-f "$(PACKAGE)-$(VERSION).tar.gz" \
		.
	-@touch -r `/bin/ls -1 -t *.c | sed -n 1,1p` "$(PACKAGE)-$(VERSION).tar.gz"
	-@/bin/ls -ld "$(PACKAGE)-$(VERSION).tar.gz"
	@test -s "$(PACKAGE)-$(VERSION).tar.gz"
