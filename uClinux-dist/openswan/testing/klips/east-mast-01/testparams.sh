#!/bin/sh

TEST_PURPOSE=goal
TEST_PROB_REPORT=0
TEST_TYPE=klipstest

TESTNAME=east-mast-01
TESTHOST=east
EXITONEMPTY=--exitonempty

THREEEIGHT=true
REF_PUB_OUTPUT=mast0-output.txt
REF_CONSOLE_OUTPUT=mast0-console.txt
REF26_CONSOLE_OUTPUT=mast0-console.txt

PUB_ARPREPLY=--arpreply

REF_CONSOLE_FIXUPS="kern-list-fixups.sed nocr.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS klips-spi-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS script-only.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS ipsec-look-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS east-prompt-splitline.pl"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS klips-debug-sanitize.sed"
INIT_SCRIPT=mast0out.sh

RUN_SCRIPT=mast0run.sh

