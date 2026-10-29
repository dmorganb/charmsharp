#!/bin/sh
set -eu
tool=$1
repo=$(pwd)
command -v charmc >/dev/null 2>&1 || { echo 'charmc required' >&2; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/pupable/Pupable.ci tests/fixtures/pupable/Pupable.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc Pupable.ci
    charmc Pupable.C -o example
    timeout 45 ./example +p2 > "$tmp/reference.out"
)
"$repo/$tool" --module Pupable --out-dir "$tmp/generated" examples/Pupable.csh
grep -Fq 'PUPable State;' "$tmp/generated/Pupable.ci"
grep -Fq 'PUPable_decl(State);' "$tmp/generated/Pupable.cpp"
(
    cd "$tmp/generated"
    charmc Pupable.ci
    charmc Pupable.cpp -o example
    timeout 45 ./example +p2 > "$tmp/generated.out"
)
grep -Fxq 'PUPable value 42 PE 1' "$tmp/reference.out"
grep -Fxq 'PUPable value 42 PE 1' "$tmp/generated.out"
diff -u "$tmp/reference.out" "$tmp/generated.out"
echo 'PUPable registration and cross-PE serialization match handwritten Charm++.'
