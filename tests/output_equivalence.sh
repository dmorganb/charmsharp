#!/usr/bin/env bash
set -euo pipefail

# Compare pre-CIEmitter (merged PR1) and current compiler outputs using the
# same source files and module names. This intentionally pins the v0.1 corpus:
# future language features need not be accepted by the frozen baseline.
if [[ $# -ne 2 ]]; then
    echo "usage: $0 CURRENT_ROOT BASELINE_ROOT" >&2
    exit 2
fi

current_root=$1
baseline_root=$2
current_bin="$current_root/build/charmsharp"
baseline_bin="$baseline_root/build/charmsharp"

if [[ ! -x "$current_bin" || ! -x "$baseline_bin" ]]; then
    echo "build both compiler revisions before running the comparison" >&2
    exit 2
fi

# The 19 tracked Charm# examples and benchmark ports at PR2. This includes
# SdagWhen, SdagSequence, SdagControlFlow, SdagExtended and Prefix Sum.
cases=(
    "benchmarks/jacobi2d/charmsharp/stencil2d.csh"
    "benchmarks/prefix-sum/charmsharp/prefix.csh"
    "benchmarks/primes/charmsharp/primes.csh"
    "benchmarks/reduction/charmsharp/reduction.csh"
    "examples/ConstructorInitializers.csh"
    "examples/Counter.csh"
    "examples/Hello.csh"
    "examples/Inheritance.csh"
    "examples/MarshalledArrays.csh"
    "examples/Message.csh"
    "examples/MessageInheritance.csh"
    "examples/Objects.csh"
    "examples/ParallelInheritance.csh"
    "examples/Readonly.csh"
    "examples/SdagControlFlow.csh"
    "examples/SdagExtended.csh"
    "examples/SdagSequence.csh"
    "examples/SdagWhen.csh"
    "examples/Threaded.csh"
)

comparison_dir=$(mktemp -d)
trap 'rm -rf "$comparison_dir"' EXIT

count=0
for source_path in "${cases[@]}"; do
    ((count += 1))
    source_file="$current_root/$source_path"
    if [[ ! -f "$source_file" ]]; then
        echo "missing comparison source: $source_path" >&2
        exit 1
    fi

    module="OutputEquivalence$count"
    old_dir="$comparison_dir/old-$count"
    new_dir="$comparison_dir/new-$count"
    mkdir -p "$old_dir" "$new_dir"

    "$baseline_bin" --module "$module" --out-dir "$old_dir" "$source_file" >/dev/null
    "$current_bin" --module "$module" --out-dir "$new_dir" "$source_file" >/dev/null

    for extension in ci cpp; do
        old_file="$old_dir/$module.$extension"
        new_file="$new_dir/$module.$extension"
        if ! cmp -s "$old_file" "$new_file"; then
            echo "BYTE MISMATCH ($extension): $source_path" >&2
            diff -u "$old_file" "$new_file" || true
            exit 1
        fi
    done
    echo "MATCH: $source_path (.ci and .cpp)"
done

echo "PASS: $count Charm# programs, $((count * 2)) generated files, byte-for-byte identical."
