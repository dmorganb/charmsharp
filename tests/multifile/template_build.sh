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

# Two independently defined templates with explicit registrations aggregate
# into one specialization module, with no duplicate Charmxi registration.
"$repo/build/charmsharp" --build --module TemplateMulti2 --out-dir "$out/combined" \
 -o "$out/multiapp" "$repo/tests/multifile/TemplateWorker.csh" \
 "$repo/tests/multifile/TemplateOther.csh" \
 "$repo/tests/multifile/TemplateOtherMain.csh"
result="$("$out/multiapp" +p2)"
printf '%s\n' "$result"
grep -q 'MULTIFILE OTHER TEMPLATE: 43' <<<"$result"
grep -q 'chare GenericWorker<int>' "$out/combined/TemplateMulti2_specializations.ci"
grep -q 'chare OtherWorker<long>' "$out/combined/TemplateMulti2_specializations.ci"
