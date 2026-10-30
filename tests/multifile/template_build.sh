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


# Confirm that real Charmxi errors in the separate specializations module map
# to the registration's original Charm# source location.
real_charmc="$(command -v charmc)"
mkdir -p "$out/wrapper"
cat >"$out/wrapper/charmc" <<'EOF'
#!/bin/sh
case "$1" in
    *_specializations.ci)
        sed 's/chare GenericWorker<int>/chare @@@ GenericWorker<int>/' "$1" >"$1.injected"
        mv "$1.injected" "$1"
        ;;
esac
exec "$CHARMSHARP_REAL_CHARMC" "$@"
EOF
chmod +x "$out/wrapper/charmc"
if PATH="$out/wrapper:$PATH" CHARMSHARP_REAL_CHARMC="$real_charmc" \
    "$repo/build/charmsharp" --build --module BadTemplateMapping \
    --out-dir "$out/bad-specializations" -o "$out/bad-template-app" \
    "$repo/tests/multifile/TemplateWorker.csh" \
    "$repo/tests/multifile/TemplateMain.csh" \
    >"$out/bad-template.log" 2>&1; then
    echo "invalid template registration unexpectedly compiled" >&2
    exit 1
fi
if ! grep -Eq "$repo/tests/multifile/TemplateWorker\.csh:[0-9]+:" \
    "$out/bad-template.log"; then
    echo "Charmxi specialization diagnostic did not map to TemplateWorker.csh" >&2
    sed -n '1,120p' "$out/bad-template.log" >&2
    exit 1
fi
