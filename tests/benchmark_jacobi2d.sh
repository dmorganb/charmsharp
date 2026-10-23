#!/bin/sh
# Compile and compare the upstream stencil/Jacobi benchmark and Charm# port.
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

cp benchmarks/jacobi2d/charmpp/stencil2d.ci benchmarks/jacobi2d/charmpp/stencil2d.C "$tmp/reference/"
(
  cd "$tmp/reference"
  charmc stencil2d.ci
  charmc stencil2d.C -o stencil2d
)

"$repo/$tool" --module stencil2d --out-dir "$tmp/charmsharp" benchmarks/jacobi2d/charmsharp/stencil2d.csh
(
  cd "$tmp/charmsharp"
  charmc stencil2d.ci
  charmc stencil2d.cpp -o stencil2d
)

# Small grid/tile size and generous threshold exercise remote halo marshalling
# and convergence reduction without a long numerical benchmark runtime.
(
  cd "$tmp/reference"
  timeout 60 ./stencil2d 8 4 0.5 +p2 > "$tmp/reference.out"
)
(
  cd "$tmp/charmsharp"
  timeout 60 ./stencil2d 8 4 0.5 +p2 > "$tmp/charmsharp.out"
)

grep -E '^(Stencil2D:|Step [0-9]+:|Converged after )' "$tmp/reference.out" > "$tmp/reference.results"
grep -E '^(Stencil2D:|Step [0-9]+:|Converged after )' "$tmp/charmsharp.out" > "$tmp/charmsharp.results"

grep -q '^Step [0-9]' "$tmp/reference.results"
grep -q '^Converged after ' "$tmp/reference.results"
diff -u "$tmp/reference.results" "$tmp/charmsharp.results"
echo "Jacobi2D benchmark: Charm# and original Charm++ outputs match."
