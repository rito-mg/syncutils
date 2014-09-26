# Nibbler

## Purpose

Nibbler (named after the Futurama character) simply eats all data given to him as quickly as he can and tells you how long it took to do it. The primary use for this is to serve as a poor man's network throughput measurement tool when all ports on the remote host are blocked except for ssh.

In that case, you can `/usr/bin/ssh user@remotehost /remote/path/to/nibbler < localhugefile` to get a rough idea of throughput, taking into account the considerable encryption overhead of ssh. When used in conjunction with `bigfilemaker`, you can be sure you're measuring just network and encryption performance rather than disk I/O.

## Usage

```
Usage: nibbler [-options] [files...] [tcp:port...] < stdin

Options:
  -b XX : Set transfer buffer size to XX (default: 8192).
  -v    : Verbose messages.
  -vv   : Even more verbose messages.
  -q    : No messages.
  -i    : Consume stdin even if other items were specified.
  -m    : Print the MD5 digest for all data consumed.
  -mm   : Print the MD5 digest for each item consumed.
  -p    : Print a dot for every 1 MB eaten.
  -P XX : Print a dot for every XX eaten.

Examples:
  echo "hello world" | nibbler
  nibbler -mm /etc/services /etc/protocols
  nibbler -v tcp:51234 &; echo "hello world" | nc localhost 51234
  dd if=/dev/urandom bs=1048576 count=10 status=none | ssh -c blowfish -o Compression=no a.example.com /usr/local/bin/nibbler -p

Version:
  1.2 * 2014-06-18 * Gleason
```
