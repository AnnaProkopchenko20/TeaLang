#!/bin/bash
# Test Runner:
# - Valid programs (tests/ok/*.txt)   -> run with -ast and compare stdout against .ast (or .expected)
# - E2E programs (tests/e2e/*.txt)    -> compile to .ll, run with lli (with optional .in), compare stdout against .expected, optional stderr against .warn
# - Invalid programs (tests/err/*.txt)-> verify non-zero exit code, no stdout, and compare stderr against .err (or .expected)

set -e
mkdir -p build
(cd build && cmake .. > /dev/null && cmake --build . --target compiler)
set +e

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0
failed_tests=()

check() {  # label actual_file expected_file
    if [ ! -f "$3" ]; then
        echo "MISSING  $1  (no $3)"; fail=$((fail+1)); failed_tests+=("$1")
    elif diff -u "$3" "$2" > "$tmp/diff"; then
        echo "PASS     $1"; pass=$((pass+1))
    else
        echo "FAIL     $1"; cat "$tmp/diff"; fail=$((fail+1)); failed_tests+=("$1")
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
        failed_tests+=("$name")
    else
        check "$name" "$tmp/actual" "$expected"
    fi
done

echo "=== Running End-to-End Tests (tests/e2e) ==="
for t in tests/e2e/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"; name=$(basename "$base")
    expected="$base.expected"
    warn="$base.warn"
    infile="$base.in"

    ./build/compiler "$t" "$tmp/out.ll" > "$tmp/stdout" 2> "$tmp/warn_actual"
    ec=$?

    if [ $ec -ne 0 ]; then
        echo "FAIL     $name (compilation failed with exit code $ec)"
        cat "$tmp/warn_actual"
        fail=$((fail+1))
        failed_tests+=("$name")
        continue
    fi

    # Check warnings if .warn file exists
    if [ -f "$warn" ]; then
        if ! diff -u "$warn" "$tmp/warn_actual" > "$tmp/diff"; then
            echo "FAIL     $name (warnings mismatch)"
            cat "$tmp/diff"
            fail=$((fail+1))
            failed_tests+=("$name")
            continue
        fi
    fi

    # Run with lli
    if [ -f "$infile" ]; then
        lli "$tmp/out.ll" < "$infile" > "$tmp/actual" 2>&1
    else
        lli "$tmp/out.ll" > "$tmp/actual" 2>&1
    fi

    check "$name" "$tmp/actual" "$expected"
done

echo "=== Running Error Tests (tests/err) ==="
for t in tests/err/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"; name=$(basename "$base")
    expected="$base.err"
    [ -f "$expected" ] || expected="$base.expected"

    ./build/compiler "$t" "$tmp/out.ll" > "$tmp/stdout" 2> "$tmp/actual"
    ec=$?

    if [ $ec -eq 0 ]; then
        echo "FAIL     $name (expected failure, but exited with 0)"
        fail=$((fail+1))
        failed_tests+=("$name")
    elif [ -s "$tmp/stdout" ]; then
        echo "FAIL     $name (produced output on stdout during compilation error)"
        fail=$((fail+1))
        failed_tests+=("$name")
    else
        check "$name" "$tmp/actual" "$expected"
    fi
done

echo "-----------------------------"
echo "passed: $pass   failed: $fail"
if [ ${#failed_tests[@]} -gt 0 ]; then
    echo "Failed tests:"
    for f in "${failed_tests[@]}"; do
        echo "  $f"
    done
fi
[ "$fail" -eq 0 ]