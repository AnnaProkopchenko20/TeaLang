#!/bin/bash
# Test Runner:
# - Valid programs (tests/ok/*.txt)   -> run with -ast and compare stdout against .ast (or .expected)
# - E2E programs (tests/e2e/*.txt)    -> compile to .ll, run with lli (with optional .in), compare stdout against .expected, optional stderr against .warn
# - Stage 3 programs (tests/opt/*.txt) -> run plain and with --optimize under lli, compare against .expected; optional .in = stdin, .sorted = compare sorted lines (threads), optional .astc = `--optimize --ast` tree
# - Invalid programs (tests/err/*.txt)-> verify non-zero exit code, no stdout, and compare stderr against .err (or .expected)

set -e
mkdir -p build
(cd build && cmake .. > /dev/null && cmake --build . --target compiler)
set +e

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0
failed_tests=()
manual=()   # tests/opt programs without .expected: not run automatically (non-deterministic/interactive)

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
        timeout 20 lli "$tmp/out.ll" < "$infile" > "$tmp/actual" 2>&1
    else
        timeout 20 lli "$tmp/out.ll" > "$tmp/actual" 2>&1
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

echo "=== Running Stage 3 Tests (tests/opt: plain, --optimize, --optimize --ast) ==="
for t in tests/opt/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"; name=$(basename "$base")
    
    # Non-deterministic / interactive programs skip automated checks
    if [ ! -f "$base.expected" ] && [ ! -f "$base.sorted" ]; then
        echo "SKIP     $name (no .expected/.sorted, run manually)"; manual+=("$name"); continue
    fi
    
    infile=/dev/null; [ -f "$base.in" ] && infile="$base.in"
    for mode in plain optimize; do
        flag=""; [ "$mode" = optimize ] && flag="--optimize"
        label="$name ($mode)"
        if ! ./build/compiler $flag "$t" "$tmp/out.ll" > /dev/null 2> "$tmp/warn_actual"; then
            echo "FAIL     $label (compilation failed)"; cat "$tmp/warn_actual"
            fail=$((fail+1)); failed_tests+=("$label"); continue
        fi
        timeout 20 lli "$tmp/out.ll" < "$infile" > "$tmp/actual" 2>&1
        
        if [ ! -f "$base.expected" ] && [ ! -f "$base.sorted" ]; then
            echo "MISSING  $label  (no $base.expected)"; fail=$((fail+1)); failed_tests+=("$label")
        elif [ -f "$base.sorted" ] && [ "$(LC_ALL=C sort "$tmp/actual")" == "$(LC_ALL=C sort "$base.sorted")" ]; then
            # .sorted: several threads print, the order of their lines is not defined
            echo "PASS     $label (sorted)"; pass=$((pass+1))
        elif [ ! -f "$base.sorted" ] && [ "$(cat "$tmp/actual")" == "$(cat "$base.expected")" ]; then
            echo "PASS     $label"; pass=$((pass+1))
        else
            echo "FAIL     $label"; diff -u "$base.expected" "$tmp/actual"
            fail=$((fail+1)); failed_tests+=("$label")
        fi
    done
    
    # Check optimized AST output
    if [ -f "$base.astc" ]; then
        ./build/compiler --optimize --ast "$t" > "$tmp/actual" 2> /dev/null
        check "$name (optimized tree)" "$tmp/actual" "$base.astc"
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
if [ ${#manual[@]} -gt 0 ]; then
    echo
    echo "Not run automatically (no .expected: unpredictable output, interactive, or could hang)."
    echo "You can run them yourself:"
    for m in "${manual[@]}"; do
        in=""; [ -f "tests/opt/$m.in" ] && in=" < tests/opt/$m.in"
        echo "  $m:  ./build/compiler [--optimize] tests/opt/$m.txt build/$m.ll && lli build/$m.ll$in"
    done
fi
[ "$fail" -eq 0 ]