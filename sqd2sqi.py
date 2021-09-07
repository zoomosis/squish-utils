#!/usr/bin/env python3

"""sqd2sqi creates a new Squish SQI file from an existing SQD file.

Useful if your .sqi file becomes corrupt, or you don't actually have one.
Some mail readers (eg. GoldED) won't read a Squish base if the .sqi file
is missing.

Usage: sqd2sqi.py input.sqd output.sqi

Written by Andrew Clarke in March 2021 and released to the public domain."""


import sys
import os
import struct


def usage():
    print(__doc__)
    sys.exit(1)


def SquishHash(s):
    """
    SquishHash() is derived from the hashpjw() function by Peter J. Weinberger of AT&T Bell Labs.

    https://en.wikipedia.org/wiki/PJW_hash_function
    """

    h = 0
    for f in s:
        h = (h << 4) + ord(f.lower())
        g = h & 0xf0000000

        if g != 0:
            h |= g >> 24
            h |= g

    return h & 0x7fffffff


def main():
    if len(sys.argv) < 3:
        usage()

    infile = sys.argv[1]
    outfile = sys.argv[2]

    if infile == outfile:
        print('Error: Output filename must not be the same as input filename.')
        sys.exit(1)

    if os.path.exists(outfile):
        print('Error: Output file already exists. Delete it before continuing.')
        sys.exit(1)

    with open(infile, 'rb') as sqd, open(outfile, 'wb') as sqi:
        sz_sqbase, = struct.unpack('<H', sqd.read(2))
        assert sz_sqbase == 0x100

        sqd.seek(104)
        first_frame, = struct.unpack('<L', sqd.read(4))

        sqd.seek(130)
        sz_sqhdr, = struct.unpack('<H', sqd.read(2))
        assert sz_sqhdr == 28

        frame_ofs = first_frame

        while frame_ofs:
            sqd.seek(frame_ofs)
            frame_id, next_frame = struct.unpack('<2L', sqd.read(8))
            assert frame_id == 0xafae4453

            sqd.seek(frame_ofs + sz_sqhdr + 40)
            toname, = struct.unpack('36s', sqd.read(36))
            # toname = ''.join(map(chr, toname))
            toname = toname.decode('cp437')

            sqd.seek(frame_ofs + sz_sqhdr + 214)
            umsgid, = struct.unpack('<L', sqd.read(4))

            sqhash = SquishHash(toname)
            print('frame_ofs: 0x%08x  umsgid: %5d  hash: 0x%08x' % (frame_ofs, umsgid, sqhash))
            out = struct.pack('<3L', frame_ofs, umsgid, sqhash)
            sqi.write(out)
            frame_ofs = next_frame

    print('Done!')


if __name__ == '__main__':
    main()
