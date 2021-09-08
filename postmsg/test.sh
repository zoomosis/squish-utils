#!/bin/sh

BASE=testecho
TYPE=squish

#BASE=netmail
#TYPE=sdm

FADDR=3:633/267
FROM="andrew clarke"
SUBJ="test"
ORIGIN="X"
OPTS="--verbose"

echo Hello world. | ./postmsg $OPTS -m"$BASE" -b"$TYPE" -o$FADDR -f"$FROM" -s"$SUBJ" -ot"$ORIGIN"
