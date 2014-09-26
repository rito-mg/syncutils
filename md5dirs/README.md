# md5dirs

## Purpose

The purpose of this program is to be able to verify that the contents of a
directory are not corrupt.  To do this, you run this program against your
master source to create MD5 signatures of each file, and then you use this
signature log to check your copies against the original signatures.

Do `md5dirs -m dir` when you want to generate the MD5 log file for each,
subdirectory, and do just `md5dirs dir` when you want to verify the files
against each subdirectory's log file.

## Usage

```
Usage:  md5dirs [-m] directory
Version: 1.2 (2011-12-16)
```
