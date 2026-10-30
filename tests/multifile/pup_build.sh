#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"$repo/build/charmsharp" --build --module PupMulti --out-dir "$out/generated" \
  -o "$out/pupapp" "$repo/tests/multifile/PupState.csh" \
  "$repo/tests/multifile/PupReceiver.csh" "$repo/tests/multifile/PupMain.csh"
result="$("$out/pupapp" +p2)"
printf '%s\n' "$result"
grep -q 'MULTIFILE PUP PASS: 42 PE 1' <<<"$result"
