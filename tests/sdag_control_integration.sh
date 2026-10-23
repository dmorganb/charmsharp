#!/bin/sh
set -eu
tool=$1
command -v charmc >/dev/null 2>&1 || { echo "charmc not found in PATH" >&2; exit 1; }
repo=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/sdag_control/control.ci tests/fixtures/sdag_control/control.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc control.ci
    charmc control.C -o control
)
"$repo/$tool" --module control --out-dir "$tmp/generated" examples/SdagControlFlow.csh
grep -Fq 'when deliver[step](int tag, int value) serial {' "$tmp/generated/control.ci"
grep -Fq 'for (step = 0; step < 2; ++step)' "$tmp/generated/control.ci"
grep -Fq 'if (total == 42)' "$tmp/generated/control.ci"
test "$(grep -Fc 'entry void deliver(int tag, int value);' "$tmp/generated/control.ci")" -eq 1
(
    cd "$tmp/generated"
    charmc control.ci
    charmc control.cpp -o control
)
for arrival in early late; do
    (
        cd "$tmp/reference"
        timeout 45 ./control "$arrival" +p2 > "$tmp/reference.out"
    )
    (
        cd "$tmp/generated"
        timeout 45 ./control "$arrival" +p2 > "$tmp/generated.out"
    )
    grep '^SDAG control:' "$tmp/reference.out" > "$tmp/reference.result"
    grep '^SDAG control:' "$tmp/generated.out" > "$tmp/generated.result"
    test "$(wc -l < "$tmp/reference.result")" -eq 1
    test "$(wc -l < "$tmp/generated.result")" -eq 1
    grep -Fxq 'SDAG control: 42' "$tmp/reference.result"
    diff -u "$tmp/reference.result" "$tmp/generated.result"
done
echo 'SDAG if/for/reference-matched sequence agrees with handwritten Charm++.'
