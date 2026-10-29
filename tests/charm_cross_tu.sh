#!/bin/sh
set -eu
tool=$1
input=$2
helper=$3
command -v charmc >/dev/null 2>&1 || { echo "charmc is required" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --module CrossTU --out-dir "$tmp" "$input"
cp "$helper" "$tmp/CrossTU_helper.cpp"
cd "$tmp"

# Separate conventional Charm++ interface generation and object compilation.
charmc CrossTU.ci
charmc CrossTU_specializations.ci
charmc -c CrossTU.cpp
charmc -c CrossTU_helper.cpp
charmc CrossTU.o CrossTU_helper.o -o CrossTU

output=$(./CrossTU +p2)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -Fq 'Cross TU worker: 42'
