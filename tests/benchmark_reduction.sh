#!/bin/sh
# Compare the untouched Charm++ Reduction tutorial and its Charm# port.
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

cp benchmarks/reduction/charmpp/reduction.ci benchmarks/reduction/charmpp/reduction.C "$tmp/reference/"
(
  cd "$tmp/reference"
  charmc reduction.ci
  charmc reduction.C -o reduction
)

"$repo/$tool" --module reduction --out-dir "$tmp/charmsharp" benchmarks/reduction/charmsharp/reduction.csh
(
  cd "$tmp/charmsharp"
  charmc reduction.ci
  charmc reduction.cpp -o reduction
)

for count in 5 10; do
  expected=$((count * (count - 1) / 2))
  (
    cd "$tmp/reference"
    timeout 45 ./reduction "$count" +p2 > "$tmp/reference.out"
  )
  (
    cd "$tmp/charmsharp"
    timeout 45 ./reduction "$count" +p2 > "$tmp/charmsharp.out"
  )

  grep -E '^(reduction: |result = )' "$tmp/reference.out" > "$tmp/reference.results"
  grep -E '^(reduction: |result = )' "$tmp/charmsharp.out" > "$tmp/charmsharp.results"
  test "$(wc -l < "$tmp/reference.results")" -eq 2
  test "$(wc -l < "$tmp/charmsharp.results")" -eq 2
  grep -Fxq "result = $expected" "$tmp/reference.results"
  grep -Fxq "result = $expected" "$tmp/charmsharp.results"
  diff -u "$tmp/reference.results" "$tmp/charmsharp.results"
done

echo "Reduction benchmark: Charm# and original Charm++ outputs match for 5 and 10 elements."
