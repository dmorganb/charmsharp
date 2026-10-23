#!/bin/sh
set -eu

tool=$1
input=$2
command -v charmc >/dev/null 2>&1 || { echo "charmc is required for runtime integration" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --module Hello --out-dir "$tmp" "$input"
cd "$tmp"
charmc Hello.ci
charmc Hello.cpp -o hello
output=$(./hello +p2)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -Fq 'Hello from Charm#'
