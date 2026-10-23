#!/bin/sh
set -eu
tool=$1
command -v charmc >/dev/null 2>&1 || { echo "charmc not found in PATH" >&2; exit 1; }
repo=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/sdag_extended/extended.ci tests/fixtures/sdag_extended/extended.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc extended.ci
    charmc extended.C -o extended
)
"$repo/$tool" --module extended --out-dir "$tmp/generated" examples/SdagExtended.csh
grep -Fq 'while (step < 2)' "$tmp/generated/extended.ci"
grep -Fq 'overlap {' "$tmp/generated/extended.ci"
grep -Fq 'case {' "$tmp/generated/extended.ci"
grep -Fq 'forall [step] (0:1, 1)' "$tmp/generated/extended.ci"
for receive in tick left right first second indexed; do
    test "$(grep -Ec "entry void ${receive}\\(" "$tmp/generated/extended.ci")" -eq 1
done
(
    cd "$tmp/generated"
    charmc extended.ci
    charmc extended.cpp -o extended
)
for arrival in early late; do
    (
        cd "$tmp/reference"
        timeout 45 ./extended "$arrival" +p2 > "$tmp/reference.out"
    )
    (
        cd "$tmp/generated"
        timeout 45 ./extended "$arrival" +p2 > "$tmp/generated.out"
    )
    grep '^SDAG extended:' "$tmp/reference.out" > "$tmp/reference.result"
    grep '^SDAG extended:' "$tmp/generated.out" > "$tmp/generated.result"
    grep -Fxq 'SDAG extended: 23' "$tmp/reference.result"
    diff -u "$tmp/reference.result" "$tmp/generated.result"
done
echo 'SDAG while/overlap/case/forall agrees with handwritten Charm++ (both arrival orders).'
