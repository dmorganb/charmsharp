#!/bin/sh
# Validate all inclusive prefixes; do not depend on asynchronous print order.
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
cp benchmarks/prefix-sum/charmpp/prefix.ci benchmarks/prefix-sum/charmpp/prefix.C "$tmp/reference/"
(
    cd "$tmp/reference"
    charmc prefix.ci
    charmc prefix.C -o prefix
)
"$repo/$tool" --module prefix --out-dir "$tmp/charmsharp" benchmarks/prefix-sum/charmsharp/prefix.csh
grep -Fq 'when passValue[dist](int stepDist, int val) serial {' "$tmp/charmsharp/prefix.ci"
grep -Fq 'for (dist = 1; dist < numElements; dist <<= 1)' "$tmp/charmsharp/prefix.ci"
grep -Fq 'entry [reductiontarget] void done();' "$tmp/charmsharp/prefix.ci"
grep -Fq 'Prefix_SDAG_CODE' "$tmp/charmsharp/prefix.cpp"
(
    cd "$tmp/charmsharp"
    charmc prefix.ci
    charmc prefix.cpp -o prefix
)

validate() {
    input=$1
    n=$2
    output=$3
    if ! awk -v n="$n" '
      /^Prefix\[/ {
        if (NF != 6 || $2 != "=" || $4 != "(expected" || $6 != "OK") bad = 1
        idx = substr($1, 8, length($1) - 8)
        # Force numeric comparisons: awk may compare substr() and -v values as strings.
        idx += 0
        val = $3
        expected_value = $5
        sub(/\)$/, "", expected_value)
        if (idx !~ /^[0-9]+$/ || val !~ /^[0-9]+$/ || expected_value !~ /^[0-9]+$/) bad = 1
        if (idx < 0 || idx >= n || seen[idx]++ || val != (idx + 1) * (idx + 2) / 2 ||
            expected_value != val) bad = 1
        print idx, val
        count++
      }
      END {
        if (bad || count != n) exit 1
      }
    ' "$input" > "$output.unsorted"; then
        echo "Invalid prefix output for N=$n: $input" >&2
        grep '^Prefix' "$input" >&2 || true
        return 1
    fi
    sort -n "$output.unsorted" > "$output"
    if test "$(wc -l < "$output")" -ne "$n"; then
        echo "Missing prefix outputs for N=$n: $input" >&2
        grep '^Prefix' "$input" >&2 || true
        return 1
    fi
}

# Multiple array sizes and processing-element counts; the original program's
# deterministic per-element random delays induce varied arrival schedules.
for pes in 1 2 4; do
    for count in 2 5 8 11; do
        echo "Checking Prefix Sum N=$count +p$pes"
        (
            cd "$tmp/reference"
            timeout 60 ./prefix "$count" "+p$pes" > "$tmp/reference.out"
        )
        (
            cd "$tmp/charmsharp"
            timeout 60 ./prefix "$count" "+p$pes" > "$tmp/charmsharp.out"
        )
        validate "$tmp/reference.out" "$count" "$tmp/reference.values"
        validate "$tmp/charmsharp.out" "$count" "$tmp/charmsharp.values"
        diff -u "$tmp/reference.values" "$tmp/charmsharp.values"
    done
done
echo 'Prefix Sum: 12 Charm++/Charm# combinations match all expected values.'
