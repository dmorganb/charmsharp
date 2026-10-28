#!/usr/bin/env bash
set -euo pipefail

# Compare the baseline's tracked fixture corpus against the current compiler.
# New language-extension examples are intentionally absent from the older
# compiler baseline and must not be included in byte-equivalence checks.
base_dir="${1:?usage: bash tests/compare_pr1_pr2_outputs.sh BASE_CHECKOUT}"
base_dir="$(cd "$base_dir" && pwd)"
pr_dir="$(pwd)"
make -s -C "$base_dir" build/charmsharp
make -s build/charmsharp

count=0
ci_matches=0
cpp_matches=0
while IFS= read -r -d '' source; do
  module="$(basename "$source" .csh)"
  case_dir="$(mktemp -d)"
  mkdir -p "$case_dir/pr1" "$case_dir/pr2"
  "$base_dir/build/charmsharp" --module "$module" --out-dir "$case_dir/pr1" "$pr_dir/$source" >/dev/null
  "$pr_dir/build/charmsharp" --module "$module" --out-dir "$case_dir/pr2" "$pr_dir/$source" >/dev/null
  for extension in ci cpp; do
    if ! cmp -s "$case_dir/pr1/$module.$extension" "$case_dir/pr2/$module.$extension"; then
      echo "BYTE MISMATCH: $source ($extension)" >&2
      diff -u "$case_dir/pr1/$module.$extension" "$case_dir/pr2/$module.$extension" >&2 || true
      exit 1
    fi
  done
  ci_matches=$((ci_matches+1))
  cpp_matches=$((cpp_matches+1))
  count=$((count+1))
  rm -rf "$case_dir"
done < <(git -C "$base_dir" ls-files -z -- 'examples/*.csh' 'benchmarks/**/*.csh' 'tests/**/*.csh')

if (( count == 0 )); then
  echo "ERROR: no Charm# fixtures found" >&2
  exit 1
fi
echo "BYTE-FOR-BYTE MATCH: $count inputs, $ci_matches .ci outputs, $cpp_matches .cpp outputs"
