#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
cp "$root"/{Simulation.ci,Worker.h,Worker.cpp,Main.cpp} "$out"/
cd "$out"
charmc Simulation.ci
charmc -c Worker.cpp -o Worker.o
charmc -c Main.cpp -o Main.o
charmc -language charm++ Main.o Worker.o -o simulation
result="$(./simulation +p2)"
printf '%s\n' "$result"
grep -q 'MULTIFILE PASS' <<<"$result"
