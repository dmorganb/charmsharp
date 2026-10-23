#!/bin/sh
set -eu
tool=$1
command -v charmc >/dev/null 2>&1 || {
    echo "charmc not found in PATH" >&2
    exit 1
}
repo=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/sdag_sequence/sequence.ci tests/fixtures/sdag_sequence/sequence.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc sequence.ci
    charmc sequence.C -o sequence
)
"$repo/$tool" --module sequence --out-dir "$tmp/generated" examples/SdagSequence.csh
grep -Fq 'entry void deliver(int value);' "$tmp/generated/sequence.ci"
grep -Fq 'entry void confirm(int value);' "$tmp/generated/sequence.ci"
grep -Fq 'Worker_SDAG_CODE' "$tmp/generated/sequence.cpp"
test "$(grep -c 'serial {' "$tmp/generated/sequence.ci")" -eq 5
(
    cd "$tmp/generated"
    charmc sequence.ci
    charmc sequence.cpp -o sequence
)
for arrival in early late; do
    (
        cd "$tmp/reference"
        timeout 45 ./sequence "$arrival" +p2 > "$tmp/reference.out"
    )
    (
        cd "$tmp/generated"
        timeout 45 ./sequence "$arrival" +p2 > "$tmp/generated.out"
    )
    grep '^SDAG sequence:' "$tmp/reference.out" > "$tmp/reference.result"
    grep '^SDAG sequence:' "$tmp/generated.out" > "$tmp/generated.result"
    test "$(wc -l < "$tmp/reference.result")" -eq 1
    test "$(wc -l < "$tmp/generated.result")" -eq 1
    grep -Fxq 'SDAG sequence: 42' "$tmp/reference.result"
    diff -u "$tmp/reference.result" "$tmp/generated.result"
done
echo 'SDAG sequence: implicit serial runs match handwritten Charm++.'
