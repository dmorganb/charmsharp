#!/bin/sh
set -eu
compiler="$1"
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cat >"$tmp/Main.csh" <<'EOF'
mainchare Main {
entry:
    Main(CkArgMsg* msg) { delete msg; }
};
EOF
cat >"$tmp/Worker.csh" <<'EOF'
chare Worker {
entry:
    Worker() {}
    void run() { CkExit(); }
};
EOF
"$compiler" --module Simulation --out-dir "$tmp/generated" "$tmp/Main.csh" "$tmp/Worker.csh"
test -f "$tmp/generated/Simulation.ci"
test -f "$tmp/generated/Main.cpp"
test -f "$tmp/generated/Worker.cpp"
grep -Fq '#line 1 "' "$tmp/generated/Worker.cpp"
test -f "$tmp/generated/Simulation_classes.h"
test -f "$tmp/generated/Simulation_registry.cpp"
grep -q 'class Worker' "$tmp/generated/Simulation_classes.h"
! grep -q 'Simulation.def.h' "$tmp/generated/Worker.cpp"
grep -q 'mainmodule Simulation' "$tmp/generated/Simulation.ci"
grep -q 'mainchare Main' "$tmp/generated/Simulation.ci"
grep -q 'chare Worker' "$tmp/generated/Simulation.ci"
if "$compiler" --module Simulation --out-dir "$tmp/generated" "$tmp/Main.csh" "$tmp/Worker.csh" >"$tmp/log" 2>&1; then
  echo 'missing overwrite protection' >&2
  exit 1
fi
cp "$tmp/Main.csh" "$tmp/Duplicate.csh"
if "$compiler" --force --module Simulation --out-dir "$tmp/generated" "$tmp/Main.csh" "$tmp/Duplicate.csh" >"$tmp/log" 2>&1; then
  echo 'duplicate mainchare was accepted' >&2
  exit 1
fi
cat >"$tmp/Repeated.csh" <<'EOF'
chare Worker {
entry:
    Worker() {}
};
EOF
if "$compiler" --module Simulation --out-dir "$tmp/repeated" "$tmp/Worker.csh" "$tmp/Repeated.csh" >"$tmp/log" 2>&1; then
  echo 'duplicate cross-file chare was accepted' >&2
  exit 1
fi
grep -q 'duplicate Charm# declaration' "$tmp/log"
grep -Fq "$tmp/Repeated.csh:1:7:" "$tmp/log"
grep -Fq "$tmp/Worker.csh:1:7: note: previous declaration is here" "$tmp/log"
cat >"$tmp/Invalid.csh" <<'EOF'
chare {
};
EOF
if "$compiler" --module Simulation --out-dir "$tmp/invalid" "$tmp/Main.csh" "$tmp/Invalid.csh" >"$tmp/log" 2>&1; then
  echo 'invalid second source was accepted' >&2
  exit 1
fi
grep -Fq "$tmp/Invalid.csh:1:" "$tmp/log"
grep -q 'expected a parallel type name' "$tmp/log"
mkdir -p "$tmp/other"
cp "$tmp/Worker.csh" "$tmp/other/Worker.csh"
if "$compiler" --module Simulation --out-dir "$tmp/collide" "$tmp/Worker.csh" "$tmp/other/Worker.csh" >"$tmp/log" 2>&1; then
  echo 'duplicate output basename was accepted' >&2
  exit 1
fi
# -o --build should not run charmc before recognizing output collisions.
grep -Eq 'duplicate Charm# declaration|duplicate multi-file output basename' "$tmp/log"

# A fake Charmxi failure checks location remapping without depending on charmc
# availability in the unit-test matrix.
mkdir -p "$tmp/fake-bin"
cat >"$tmp/fake-bin/charmc" <<'EOF'
#!/bin/sh
line=$(grep -n 'chare Worker' Simulation.ci | cut -d: -f1)
printf 'Simulation.ci:%s:1: error: synthetic Charmxi error\n' "$line" >&2
printf 'an unrelated diagnostic line\n' >&2
exit 1
EOF
chmod +x "$tmp/fake-bin/charmc"
if PATH="$tmp/fake-bin:$PATH" "$compiler" --build -o "$tmp/fake-executable" \
     --module Simulation --out-dir "$tmp/mapped" "$tmp/Main.csh" "$tmp/Worker.csh" \
     >"$tmp/log" 2>&1; then
  echo 'synthetic Charmxi error was accepted' >&2
  exit 1
fi
grep -Fq "$tmp/Worker.csh:1: (from Simulation.ci:" "$tmp/log"
grep -Fq 'error: synthetic Charmxi error' "$tmp/log"
grep -Fq 'an unrelated diagnostic line' "$tmp/log"

echo 'MULTIFILE CLI PASS'
