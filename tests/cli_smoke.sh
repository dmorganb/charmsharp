#!/bin/sh
set -eu
tool=$1
input=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"$tool" --module Smoke --out-dir "$tmp" "$input"
test -f "$tmp/Smoke.ci"
test -f "$tmp/Smoke.cpp"
"${CXX:-c++}" -std=c++20 -I tests/fake_charm -c "$tmp/Smoke.cpp" -o "$tmp/Smoke.o"
if "$tool" --module Smoke --out-dir "$tmp" "$input"; then
    echo "Expected overwrite protection to reject existing outputs" >&2
    exit 1
fi
"$tool" --force --module Smoke --out-dir "$tmp" "$input"


# Multi-file --build must reject object/output collisions before generating files
# or invoking charmc. Each failure leaves its output directory untouched.
mkdir -p "$tmp/left" "$tmp/right"
printf 'message LeftHelper { int value; };\n' >"$tmp/left/Shared.csh"
printf 'message RightHelper { int value; };\n' >"$tmp/right/Shared.csh"
printf 'int native_left() { return 1; }\n' >"$tmp/left/Physics.cpp"
printf 'int native_right() { return 2; }\n' >"$tmp/right/Physics.cpp"

expect_collision() {
    case_name=$1
    expected=$2
    shift 2
    out="$tmp/collision-$case_name"
    if "$tool" --build --module Collision --out-dir "$out" \
        -o "$tmp/executable-$case_name" "$@" >"$tmp/$case_name.log" 2>&1; then
        echo "Expected collision to reject $case_name" >&2
        exit 1
    fi
    if ! grep -q "$expected" "$tmp/$case_name.log"; then
        echo "Missing collision diagnostic for $case_name" >&2
        sed -n '1,60p' "$tmp/$case_name.log" >&2
        exit 1
    fi
    test ! -e "$out/Collision.ci"
}

expect_collision duplicate-charmsharp 'duplicate multi-file output basename' \
    "$tmp/left/Shared.csh" "$tmp/right/Shared.csh"
expect_collision duplicate-native 'object output collision' \
    "$input" "$tmp/left/Physics.cpp" "$tmp/right/Physics.cpp"

# The requested executable may not overwrite generated artifacts or objects,
# even with --force.
if "$tool" --build --force --module Collision --out-dir "$tmp/target" \
    -o "$tmp/target/Collision.ci" "$input" "$tmp/left/Physics.cpp" \
    >"$tmp/target.log" 2>&1; then
    echo "Expected executable/generated-interface collision to be rejected" >&2
    exit 1
fi
grep -q 'executable collides with generated output' "$tmp/target.log"
test ! -e "$tmp/target/Collision.ci"

if "$tool" --build --force --module SingleCollision \
    --out-dir "$tmp/single-target" -o "$tmp/single-target/SingleCollision.cpp" \
    "$input" >"$tmp/single-target.log" 2>&1; then
    echo "Expected single-file executable/generated-C++ collision" >&2
    exit 1
fi
grep -q 'executable collides with generated output' "$tmp/single-target.log"
test ! -e "$tmp/single-target/SingleCollision.cpp"
