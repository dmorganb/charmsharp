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
