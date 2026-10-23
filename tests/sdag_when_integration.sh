#!/bin/sh
# Compile the handwritten and transpiled SDAG programs with pinned Charm++.
# Exercise a queued receive and one sent after the wait is initiated.
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

cp tests/fixtures/sdag_when/when.ci tests/fixtures/sdag_when/when.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc when.ci
    charmc when.C -o when
)
"$repo/$tool" --module when --out-dir "$tmp/generated" examples/SdagWhen.csh

grep -Fq 'entry void deliver(int value);' "$tmp/generated/when.ci"
grep -Fq 'when deliver(int value) serial {' "$tmp/generated/when.ci"
grep -Fq 'Worker_SDAG_CODE' "$tmp/generated/when.cpp"
(
    cd "$tmp/generated"
    charmc when.ci
    charmc when.cpp -o when
)
for arrival in early late; do
    (
        cd "$tmp/reference"
        timeout 45 ./when "$arrival" +p2 > "$tmp/reference.out"
    )
    (
        cd "$tmp/generated"
        timeout 45 ./when "$arrival" +p2 > "$tmp/generated.out"
    )
    grep '^SDAG result:' "$tmp/reference.out" > "$tmp/reference.result"
    grep '^SDAG result:' "$tmp/generated.out" > "$tmp/generated.result"
    test "$(wc -l < "$tmp/reference.result")" -eq 1
    test "$(wc -l < "$tmp/generated.result")" -eq 1
    grep -Fxq 'SDAG result: 42' "$tmp/reference.result"
    diff -u "$tmp/reference.result" "$tmp/generated.result"
done

echo 'SDAG when: handwritten and transpiled Charm++ agree for early and late arrival.'
