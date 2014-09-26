# Bigfilemaker

## Purpose

There are occasions when it is desirable to create large junk files without the overhead of disk I/O (creating from another file) or context switches (as you would get using `/dev/zero` or `/dev/urandom` with `/usr/bin/dd`). It can also be helpful to generate a checksum for the file on the fly, rather than having a program like `md5sum` re-read the newly generated file.

Bigfilemaker is well-suited for quickly generating these large test files and writing them to disk or to a socket.
