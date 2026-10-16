#!/bin/bash
# Stage 1 Test Runner:
# - Valid programs (tests/ok/*.txt)   -> run with -ast and compare stdout against .ast (or .expected)
# - Invalid programs (tests/err/*.txt) -> verify non-zero exit code, no stdout, and compare stderr against .err (or .expected)

set -e
mkdir -p build
(cd build && cmake .. > /dev/null && cmake --build . --target compiler)
set +e

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

check() {  # label actual_file expected_file
    if [ ! -f "$3" ]; then
        echo "MISSING  $1  (no $3)"; fail=$((fail+1))
    elif diff -u "$3" "$2" > "$tmp/diff"; then
        echo "PASS     $1"; pass=$((pass+1))
    else
        echo "FAIL     $1"; cat "$tmp/diff"; fail=$((fail+1))
    fi
}

echo "=== Running Happy-Path Tests (tests/ok) ==="
for t in tests/ok/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"; name=$(basename "$base")
    expected="$base.ast"
    [ -f "$expected" ] || expected="$base.expected"

    ./build/compiler -ast "$t" > "$tmp/actual" 2> "$tmp/err"
    ec=$?

    if [ $ec -ne 0 ]; then
        echo "FAIL     $name (unexpected exit code $ec)"
        cat "$tmp/err"
        fail=$((fail+1))
    else
        check "$name" "$tmp/actual" "$expected"
    fi
done

echo "=== Running Error Tests (tests/err) ==="
for t in tests/err/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"; name=$(basename "$base")
    expected="$base.err"
    [ -f "$expected" ] || expected="$base.expected"

    ./build/compiler -ast "$t" > "$tmp/stdout" 2> "$tmp/actual"
    ec=$?

    if [ $ec -eq 0 ]; then
        echo "FAIL     $name (expected failure, but exited with 0)"
        fail=$((fail+1))
    elif [ -s "$tmp/stdout" ]; then
        echo "FAIL     $name (produced output on stdout during compilation error)"
        fail=$((fail+1))
    else
        check "$name" "$tmp/actual" "$expected"
    fi
done

echo "-----------------------------"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]