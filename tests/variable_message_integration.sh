#!/bin/sh
set -eu
tool=$1
command -v charmc >/dev/null 2>&1 || { echo 'charmc required' >&2; exit 1; }
repo=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/variable_message/VariableMessage.ci tests/fixtures/variable_message/VariableMessage.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc VariableMessage.ci
    charmc VariableMessage.C -o example
    timeout 45 ./example +p2 > "$tmp/reference.out"
)
"$repo/$tool" --module VariableMessage --out-dir "$tmp/generated" examples/VariableMessage.csh
grep -Fq 'double values[];' "$tmp/generated/VariableMessage.ci"
grep -Fq 'int indices[];' "$tmp/generated/VariableMessage.ci"
(
    cd "$tmp/generated"
    charmc VariableMessage.ci
    charmc VariableMessage.cpp -o example
    timeout 45 ./example +p2 > "$tmp/generated.out"
)
grep '^Variable message:' "$tmp/reference.out" > "$tmp/reference.result"
grep '^Variable message:' "$tmp/generated.out" > "$tmp/generated.result"
test "$(wc -l < "$tmp/reference.result")" -eq 1
test "$(wc -l < "$tmp/generated.result")" -eq 1
grep -Fxq 'Variable message: PASS on PE 1' "$tmp/reference.result"
diff -u "$tmp/reference.result" "$tmp/generated.result"
for variant in Single Unequal; do
    mkdir -p "$tmp/$variant"
    "$repo/$tool" --build --module "VariableMessage$variant" --out-dir "$tmp/$variant" -o "$tmp/$variant/example" "examples/VariableMessage$variant.csh"
    ( cd "$tmp/$variant"; timeout 45 ./example +p2 ) > "$tmp/$variant.out"
done
grep -Fxq 'Single variable message: PASS on PE 1' "$tmp/Single.out"
grep -Fxq 'Unequal variable message: PASS on PE 1' "$tmp/Unequal.out"
echo 'Variable message reference, one-array, zero-length, unequal sizes, and remote cleanup: PASS'
