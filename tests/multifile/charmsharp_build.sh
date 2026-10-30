#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"$repo/build/charmsharp" --module Simulation --out-dir "$out" \
    "$repo/tests/multifile/Main.csh" "$repo/tests/multifile/Worker.csh"
cd "$out"
charmc Simulation.ci
charmc -I"$repo/tests/multifile" -c Main.cpp -o Main.o
charmc -I"$repo/tests/multifile" -c Worker.cpp -o Worker.o
charmc -I"$repo/tests/multifile" -c Simulation_registry.cpp -o registry.o
charmc -I"$repo/tests/multifile" -c "$repo/tests/multifile/Physics.cpp" -o Physics.o
charmc -language charm++ Main.o Worker.o registry.o Physics.o -o simulation
output="$(./simulation +p2)"
printf '%s\n' "$output"
grep -q 'PHYSICS 42' <<<"$output"
grep -q 'MULTIFILE CHARM# PASS: 42' <<<"$output"

# Also exercise the integrated project --build driver on the same sources.
"$repo/build/charmsharp" --build --module Simulation --out-dir "$out/integrated" \
    -o "$out/managed" \
    "$repo/tests/multifile/Main.csh" "$repo/tests/multifile/Worker.csh" \
    "$repo/tests/multifile/Physics.cpp"
managed_output="$("$out/managed" +p2)"
printf '%s\n' "$managed_output"
grep -q 'PHYSICS 42' <<<"$managed_output"
grep -q 'MULTIFILE CHARM# PASS: 42' <<<"$managed_output"


# Errors in both a moved chare definition and untouched C++ body should
# refer to the originating Charm# file, not generated C++/header locations.
cat >"$out/Broken.csh" <<'EOF'
chare Broken {
entry:
    Broken() { missing_member = 1; }
};
message BrokenMessage {
    int values[];
    void broken_member() { missing_message_member = 1; }
};
void broken_cpp() { missing_function(); }
EOF
cat >"$out/Other.csh" <<'EOF'
chare Other {
entry:
    Other() {}
};
EOF
"$repo/build/charmsharp" --module BadSimulation --out-dir "$out/bad" \
    "$out/Broken.csh" "$out/Other.csh"
(
    cd "$out/bad"
    charmc BadSimulation.ci
    if charmc -c Broken.cpp -o Broken.o >"$out/compiler_errors.log" 2>&1; then
        echo "invalid Charm# code unexpectedly compiled" >&2
        exit 1
    fi
)
grep -Eq 'Broken\.csh:3(:[0-9]+)?:' "$out/compiler_errors.log"
grep -Eq 'Broken\.csh:9(:[0-9]+)?:' "$out/compiler_errors.log"
grep -Eq 'Broken\.csh:7(:[0-9]+)?:' "$out/compiler_errors.log"


# Exercise real Charmxi error formatting through the Charm# --build driver.
# The wrapper injects a malformed token into a generated interface immediately
# before invoking the installed pinned Charm++ toolchain.
real_charmc="$(command -v charmc)"
mkdir -p "$out/charmxi-wrapper"
cat >"$out/charmxi-wrapper/charmc" <<'EOF'
#!/bin/sh
case "$CHARMSHARP_TEST_BAD_DECL" in
    Main) sed 's/mainchare Main {/mainchare @@@ Main {/' "$1" >"$1.injected" ;;
    Worker) sed 's/chare Worker {/chare @@@ Worker {/' "$1" >"$1.injected" ;;
    *) exit 3 ;;
esac
mv "$1.injected" "$1"
exec "$CHARMSHARP_REAL_CHARMC" "$@"
EOF
chmod +x "$out/charmxi-wrapper/charmc"
for declaration in Main Worker; do
    if PATH="$out/charmxi-wrapper:$PATH" \
        CHARMSHARP_REAL_CHARMC="$real_charmc" \
        CHARMSHARP_TEST_BAD_DECL="$declaration" \
        "$repo/build/charmsharp" --build --module BrokenXi \
        --out-dir "$out/charmxi-$declaration" -o "$out/invalid-$declaration" \
        "$repo/tests/multifile/Main.csh" "$repo/tests/multifile/Worker.csh" \
        >"$out/charmxi-$declaration.log" 2>&1; then
        echo "real Charmxi unexpectedly accepted malformed $declaration" >&2
        exit 1
    fi
    if ! grep -Eq "$repo/tests/multifile/$declaration\.csh:[0-9]+:" \
        "$out/charmxi-$declaration.log"; then
        echo "Charmxi did not map $declaration to its source:" >&2
        sed -n '1,120p' "$out/charmxi-$declaration.log" >&2
        exit 1
    fi
done


# A malformed statement inside an SDAG receive must map to that exact
# Charm# source line, not merely the enclosing entry method.
mkdir -p "$out/sdagxi-wrapper"
cat >"$out/sdagxi-wrapper/charmc" <<'EOF'
#!/bin/sh
case "$1" in
    BadSdagXi.ci)
        sed 's/parent.done(value);/@@@ parent.done(value);/' "$1" >"$1.injected"
        mv "$1.injected" "$1"
        ;;
esac
exec "$CHARMSHARP_REAL_CHARMC" "$@"
EOF
chmod +x "$out/sdagxi-wrapper/charmc"
if PATH="$out/sdagxi-wrapper:$PATH" CHARMSHARP_REAL_CHARMC="$real_charmc" \
    "$repo/build/charmsharp" --build --module BadSdagXi \
    --out-dir "$out/bad-sdagxi" -o "$out/invalid-sdag" \
    "$repo/examples/SdagWhen.csh" "$out/Other.csh" \
    >"$out/sdagxi.log" 2>&1; then
    echo "malformed generated SDAG statement unexpectedly compiled" >&2
    exit 1
fi
expected_sdag_line="$(grep -n 'parent.done(value);' "$repo/examples/SdagWhen.csh" | cut -d: -f1)"
if ! grep -Fq "$repo/examples/SdagWhen.csh:$expected_sdag_line:" "$out/sdagxi.log"; then
    echo "Charmxi SDAG error did not map to the original statement:" >&2
    sed -n '1,120p' "$out/sdagxi.log" >&2
    exit 1
fi
