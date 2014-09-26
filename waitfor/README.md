# Waitfor

## Purpose

This utility is primarily used to test latency between remote replication nodes. For example,
a source host may have a continually running rsync (or periodically relaunched), and you want
to measure how long it takes for a newly created file on the source to be replicated on the
target.

## Usage
```
Usage: waitfor [-c|-d] [-options] 'file1[,size1]' [files...]

Options:
  -d     : Wait for files to be deleted rather than created.
  -c     : Wait for files to be created [to size] (default mode).
  -a XX  : Abort after XX seconds.
  -w X.Y : Wait X.Y seconds between checks (default: 2.00 sec).
  -v     : Verbose messages.
  -q     : No messages.
  -l XX  : Append to log file XX with results.
  -D X   : Delimit pathnames and expected sizes by character X rather than a comma.

Examples:
  waitfor -c -w 0.5 -a 120 /tmp/bigfile,100M /tmp/file2,32768
  waitfor -d -w 0.5 -a 120 /tmp/bigfile /tmp/file2

Version:
  1.0 * 2014-04-02 * Gleason
```
