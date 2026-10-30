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
