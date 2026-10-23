#!/bin/sh
# Build the unmodified Charm++ reference and Charm# port with the same toolchain.
set -eu

tool=$1
command -v charmc >/dev/null 2>&1 || {
  echo "charmc not found in PATH" >&2
  exit 1
}

repo=$(pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/reference" "$tmp/charmsharp"

cp benchmarks/primes/charmpp/primes.ci benchmarks/primes/charmpp/primes.C "$tmp/reference/"
(
  cd "$tmp/reference"
  charmc primes.ci
  charmc primes.C -o primes
)

"$repo/$tool" --module primes --out-dir "$tmp/charmsharp" benchmarks/primes/charmsharp/primes.csh
(
  cd "$tmp/charmsharp"
  charmc primes.ci
  charmc primes.cpp -o primes
)

# K=5 exercises both prime checks and completion callbacks while staying fast:
# later random values may be prime, causing the upstream O(n) test to run long.
(
  cd "$tmp/reference"
  timeout 45 ./primes 5 +p2 > "$tmp/reference.out"
)
(
  cd "$tmp/charmsharp"
  timeout 45 ./primes 5 +p2 > "$tmp/charmsharp.out"
)

# Strip Charm++ runtime banners; compare only the five ordered results.
grep -E '^[0-9]+: (prime|not prime)$' "$tmp/reference.out" > "$tmp/reference.results"
grep -E '^[0-9]+: (prime|not prime)$' "$tmp/charmsharp.out" > "$tmp/charmsharp.results"

test "$(wc -l < "$tmp/reference.results")" -eq 5
test "$(wc -l < "$tmp/charmsharp.results")" -eq 5
diff -u "$tmp/reference.results" "$tmp/charmsharp.results"
echo "Primes benchmark: 5 outputs match the unmodified Charm++ reference."
