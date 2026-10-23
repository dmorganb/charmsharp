#!/bin/sh
set -eu
tool=$1
input=$2
if ! command -v charmc >/dev/null 2>&1; then
    echo "SKIP Charm++ integration: charmc is not installed"
    exit 0
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --module CounterModule --out-dir "$tmp" "$input"
cd "$tmp"
charmc CounterModule.ci
charmc -c CounterModule.cpp
