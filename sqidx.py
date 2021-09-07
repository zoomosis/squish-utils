#!/usr/bin/env python3

"""sqidx.py: Create an index of messages in a Squish message base in CSV format.

Usage: sqidx.py base

Written in 2008 by Andrew Clarke and released to the public domain.

This version was rewritten in Python and supercedes the older C version, sqidx.c."""

import sys
import struct
import time

SQHDRID = 0xafae4453
MSG_LIMIT = 500000
CODEC = 'cp437'


def usage():
    print(__doc__)
    sys.exit(1)


def str_hash(key):
    h = 0
    for c in key:
        h = (h << 5) - h + ord(c)
    return h & 0xffffffff  ### truncate to 32-bits


def show_header(fp, frame_ofs):
    ### get from/to/subject lines; truncate from '\0' onwards

    fp.seek(frame_ofs + 28 + 4)
    msg_from = fp.read(36).decode(CODEC).split('\0')[0]
    msg_to = fp.read(36).decode(CODEC).split('\0')[0]
    msg_subject = fp.read(72).decode(CODEC).split('\0')[0]

    ### get time & date the message was written

    fp.seek(frame_ofs + 28 + 164)
    idate = struct.unpack('<H', fp.read(2))[0]
    itime = struct.unpack('<H', fp.read(2))[0]

    year = ((idate >> 9) & 0x7f) + 1980

    dt = (year, \
      ((idate >> 5) & 0x0f), \
      idate & 0x1f, \
      (itime >> 11) & 0x1f, \
      (itime >> 5) & 0x3f, \
      (itime & 0x1f) << 1, 0, 0, 0)

    t = time.mktime(dt)

    lt = time.localtime(t)

    str_date = time.strftime("%Y-%m-%d %H:%M:%S", lt)

    hash_combo = "%08lx%08lx" % \
      (str_hash(str_date), \
      (str_hash(msg_from) + str_hash(msg_to) + str_hash(msg_subject)) & 0xffffffff)

    ### "FrameOffset","Hash","FromName","ToName","Subject","DateTime"

    print('"%lu","%s","%s","%s","%s","%s"' % \
      (frame_ofs, hash_combo, escape_quotes(msg_from), escape_quotes(msg_to),
      escape_quotes(msg_subject), str_date))


def escape_quotes(s):
    s = s.replace('"', '""')
    s = s.replace('\\', '\\\\')
    return s


def traverse_frame_list(fp, frame_ofs):
    msg_num = 0

    while frame_ofs != 0 and msg_num < MSG_LIMIT:
        msg_num += 1
        fp.seek(frame_ofs)

        ### validate this is a real frame

        hdrid = struct.unpack('<L', fp.read(4))[0]
        assert hdrid == SQHDRID

        ### get offset to previous message frame

        fp.seek(frame_ofs + 8)
        prev_frame = struct.unpack('<L', fp.read(4))[0]

        ### if not a normal message (eg. it has been deleted), skip it

        fp.seek(frame_ofs + 24)
        frame_type = struct.unpack('<H', fp.read(2))[0]

        if frame_type == 0:
            show_header(fp, frame_ofs)

        frame_ofs = prev_frame


def get_sqbase(base):
    sqd_fn = base + '.sqd'

    with open(sqd_fn, 'rb') as fp:
        sz_sqbase = struct.unpack('<H', fp.read(2))[0]
        assert sz_sqbase == 256

        ### get offset to the last message frame in the base

        fp.seek(108)
        frame_ofs = struct.unpack('<L', fp.read(4))[0]

        ### traverse backwards through each frame in the base

        traverse_frame_list(fp, frame_ofs)


def main():
    if len(sys.argv) < 2:
        usage()

    get_sqbase(sys.argv[1])


if __name__ == '__main__':
    main()
