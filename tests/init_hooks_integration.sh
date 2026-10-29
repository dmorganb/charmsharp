#!/bin/sh
set -eu
tool=$1
repo=$(pwd)
command -v charmc >/dev/null 2>&1 || { echo 'charmc required' >&2; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/generated"
cp tests/fixtures/init_hooks/InitHooks.ci tests/fixtures/init_hooks/InitHooks.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc InitHooks.ci
    charmc InitHooks.C -o example
    timeout 45 ./example +p2 > "$tmp/reference.out"
)
"$repo/$tool" --module InitHooks --out-dir "$tmp/generated" examples/InitHooks.csh
grep -Fq 'initnode void initializeNode();' "$tmp/generated/InitHooks.ci"
grep -Fq 'initproc void initializeProcessor();' "$tmp/generated/InitHooks.ci"
(
    cd "$tmp/generated"
    charmc InitHooks.ci
    charmc InitHooks.cpp -o example
    timeout 45 ./example +p2 > "$tmp/generated.out"
)
for version in reference generated; do
    # Initialization hooks must precede mainchare construction.
    awk '/^Hook main PE / { main = NR }
         /^Hook node / || /^Hook processor / { if (main) exit 1 }
         END { if (!main) exit 1 }' "$tmp/$version.out"
    grep '^Hook ' "$tmp/$version.out" | sort > "$tmp/$version.result"
    test "$(grep -c '^Hook node 0$' "$tmp/$version.result")" -eq 1
    test "$(grep -c '^Hook processor 0$' "$tmp/$version.result")" -eq 1
    test "$(grep -c '^Hook processor 1$' "$tmp/$version.result")" -eq 1
    test "$(grep -c '^Hook main PE 0$' "$tmp/$version.result")" -eq 1
    test "$(wc -l < "$tmp/$version.result")" -eq 4
done
diff -u "$tmp/reference.result" "$tmp/generated.result"
echo 'Initialization hooks: node and PE invocation counts match pinned Charm++.'
