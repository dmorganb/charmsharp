#!/bin/sh
set -eu

tool=$1
input=$2
module=${3:-Hello}
expected=${4:-Hello from Charm#}
command -v charmc >/dev/null 2>&1 || { echo "charmc is required for runtime integration" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --build --module "$module" --out-dir "$tmp" -o "$tmp/example" "$input"
cd "$tmp"
output=$(./example +p2)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -Fq "$expected"

