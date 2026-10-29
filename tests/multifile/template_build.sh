#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"$repo/build/charmsharp" --build --module TemplateMulti --out-dir "$out/generated" \
 -o "$out/templateapp" "$repo/tests/multifile/TemplateWorker.csh" \
 "$repo/tests/multifile/TemplateMain.csh"
result="$("$out/templateapp" +p2)"
printf '%s\n' "$result"
grep -q 'MULTIFILE TEMPLATE PASS: 42' <<<"$result"
