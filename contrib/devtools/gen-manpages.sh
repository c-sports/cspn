#!/usr/bin/env bash

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

CSPND=${CSPND:-$BINDIR/cspnd}
CSPNCLI=${CSPNCLI:-$BINDIR/cspn-cli}
CSPNTX=${CSPNTX:-$BINDIR/cspn-tx}
WALLET_TOOL=${WALLET_TOOL:-$BINDIR/cspn-wallet}
CSPNQT=${CSPNQT:-$BINDIR/qt/cspn-qt}

[ ! -x $CSPND ] && echo "$CSPND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
read -r -a BTCVER <<< "$($CSPNCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }')"

# Create a footer file with copyright content.
# This gets autodetected fine for cspnd if --version-string is not set,
# but has different outcomes for cspn-qt and cspn-cli.
echo "[COPYRIGHT]" > footer.h2m
$CSPND --version | sed -n '1!p' >> footer.h2m

for cmd in $CSPND $CSPNCLI $CSPNTX $WALLET_TOOL $CSPNQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
