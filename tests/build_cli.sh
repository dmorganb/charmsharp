#!/bin/sh
set -eu
# Keep paths valid after changing directory without requiring realpath (absent
# on older macOS versions). These paths need not have symlinks resolved.
case "$1" in
    /*) tool=$1 ;;
    *) tool=$PWD/$1 ;;
esac
case "$2" in
    /*) input=$2 ;;
    *) input=$PWD/$2 ;;
esac
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/bin"
export BUILD_LOG="$tmp/log"
cat > "$tmp/bin/charmc" <<'FAKE'
#!/bin/sh
set -eu
printf '%s\n' "$PWD" "$@" >> "$BUILD_LOG"
case "$1" in
    *.ci) test -f "$1"; exit "${CI_STATUS:-0}" ;;
    *.cpp)
        test -f "$1"
        [ "${CPP_STATUS:-0}" = 0 ] || exit "$CPP_STATUS"
        shift
        while [ "$#" -gt 0 ]; do
            if [ "$1" = -o ]; then shift; : > "$1"; exit 0; fi
            shift
        done
        exit 99 ;;
esac
exit 98
FAKE
chmod +x "$tmp/bin/charmc"
export PATH="$tmp/bin:$PATH"
cd "$tmp"
# Shell metacharacters must remain literal arguments, including the executable path.
"$tool" --build --out-dir 'generated files' -o 'hello; echo injected' "$input"
test -f 'hello; echo injected'
test ! -e injected
test "$(wc -l < "$BUILD_LOG")" -eq 7
sed -n '2p' "$BUILD_LOG" | grep -Fx Hello.ci
sed -n '4p' "$BUILD_LOG" | grep -Fx Hello.cpp
sed -n '5p' "$BUILD_LOG" | grep -Fx -- "-I$(dirname "$input")"
# Refuse overwrite before launching any subprocess.
if "$tool" --build --out-dir other -o 'hello; echo injected' "$input"; then exit 1; fi
test ! -d other
"$tool" --build --force --out-dir 'generated files' -o 'hello; echo injected' "$input"
# Default executable is in the caller's working directory.
"$tool" --build --out-dir default "$input"
test -f Hello
if "$tool" -o unused "$input"; then exit 1; fi
if "$tool" --build -o; then exit 1; fi
: > "$BUILD_LOG"
if CI_STATUS=7 "$tool" --build --out-dir ci-fail -o failed "$input" 2>err; then exit 1; fi
grep -F 'exit status 7' err
test "$(wc -l < "$BUILD_LOG")" -eq 2
test ! -e failed
if CPP_STATUS=9 "$tool" --build --out-dir cpp-fail -o failed "$input" 2>err; then exit 1; fi
grep -F 'exit status 9' err
test ! -e failed
# Translation failure must never invoke charmc.
: > "$BUILD_LOG"
printf 'chare {' > invalid.csh
if "$tool" --build invalid.csh; then exit 1; fi
test ! -s "$BUILD_LOG"
# Deliberately remove every charmc candidate from PATH.
if PATH="$tmp/empty" "$tool" --build --out-dir missing -o failed "$input" 2>err; then exit 1; fi
grep -F 'cannot execute charmc' err
