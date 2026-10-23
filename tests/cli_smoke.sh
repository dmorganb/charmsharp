#!/bin/sh
set -eu
tool=$1
input=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --module Smoke --out-dir "$tmp" "$input"
test -f "$tmp/Smoke.ci"
test -f "$tmp/Smoke.cpp"
c++ -std=c++20 -I tests/fake_charm -c "$tmp/Smoke.cpp" -o "$tmp/Smoke.o"
if "$tool" --module Smoke --out-dir "$tmp" "$input"; then
    echo "Expected overwrite protection to reject existing outputs" >&2
    exit 1
fi
"$tool" --force --module Smoke --out-dir "$tmp" "$input"
