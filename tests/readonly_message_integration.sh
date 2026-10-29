#!/bin/sh
set -eu
tool=$1
repo=$(pwd)
command -v charmc >/dev/null 2>&1 || { echo 'charmc required' >&2; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/readonly_message/ReadonlyMessage.ci tests/fixtures/readonly_message/ReadonlyMessage.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc ReadonlyMessage.ci
    charmc ReadonlyMessage.C -o example
    timeout 45 ./example +p2 > "$tmp/reference.out"
)
"$repo/$tool" --module ReadonlyMessage --out-dir "$tmp/generated" examples/ReadonlyMessage.csh
grep -Fq 'readonly message SharedConfig* configuration;' "$tmp/generated/ReadonlyMessage.ci"
grep -Fq 'SharedConfig* configuration;' "$tmp/generated/ReadonlyMessage.cpp"
(
    cd "$tmp/generated"
    charmc ReadonlyMessage.ci
    charmc ReadonlyMessage.cpp -o example
    timeout 45 ./example +p2 > "$tmp/generated.out"
)
grep '^Readonly message:' "$tmp/reference.out" > "$tmp/reference.result"
grep '^Readonly message:' "$tmp/generated.out" > "$tmp/generated.result"
test "$(wc -l < "$tmp/reference.result")" -eq 1
test "$(wc -l < "$tmp/generated.result")" -eq 1
grep -Fxq 'Readonly message: PASS on PE 1' "$tmp/reference.result"
diff -u "$tmp/reference.result" "$tmp/generated.result"
echo 'Readonly message pointer: pinned Charm++ reference and generated output agree.'
