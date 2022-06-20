#!/bin/sh

APPDIR=/opt/xlnx-app-kr260-mv-defect-detect

tmpdir="$(mktemp -d)"
if [ -z "$tmpdir" ]; then
    echo "Couldn't create temp dir" >&2
    exit 1
fi
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT

cd "$tmpdir"

ln -s "$APPDIR/xgvrd-kr260.xml"
cp "$APPDIR/bin/eeprom.bin" .

"$APPDIR/bin/update_eeprom"
