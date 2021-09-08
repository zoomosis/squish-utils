squish-utils
============

Free source code for various utilities to work with Squish message base
files.


The Squish format was developed by Scott Dudley in the early 1990s for the
[Maximus BBS](https://github.com/sdudley/maximus) software and is commonly
used by [FidoNet](https://en.wikipedia.org/wiki/FidoNet) BBS sysops,
particularly when using software from the [Husky project](https://github.com/huskyproject).

squ2mbox.c: Converts Squish messagebases to UNIX mbox format.

sqidx.py: Create an index of messages in a Squish base in CSV format. Supercedes sqidx.c.

sqidx.c: Create an index of messages in a Squish base in CSV format. Obsoleted by sqidx.py.

squid.c: Display information about a Squish base.

sqd2sqi.py: Create a new Squish SQI file from an existing SQD file.

postmsg/postmsg.c: Post a message from stdin to a Squish or FTS-1 *.MSG message base.
