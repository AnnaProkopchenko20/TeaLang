#!/usr/bin/env bash
# Runs every tests/opt/*.txt twice (plain and --optimize) and compares the program
# output with NAME.expected. If NAME.astc exists, also compares `--optimize --ast`.
#   BIN=build/compiler tests/opt/run_opt.sh        (default BIN: build/compiler)
#   GEN=1 tests/opt/run_opt.sh                     writes/overwrites NAME.astc - READ THEM
cd "$(dirname "$0")/../.."
BIN=${BIN:-build/compiler}
fail=0
for t in tests/opt/*.txt; do
  n=${t%.txt}
  inp=/dev/null; [ -f "$n.in" ] && inp="$n.in"
  for mode in plain opt; do
    flag=""; [ "$mode" = opt ] && flag="--optimize"
    if ! $BIN $flag "$t" /tmp/_tea_t.ll 2>/dev/null; then echo "FAIL compile: $n ($mode)"; fail=1; continue; fi
    out=$(lli /tmp/_tea_t.ll < "$inp")
    if [ -f "$n.sorted" ]; then a=$(printf '%s\n' "$out" | LC_ALL=C sort); e=$(LC_ALL=C sort "$n.expected"); else a="$out"; e=$(cat "$n.expected"); fi
    if [ "$a" != "$e" ]; then echo "FAIL output:  $n ($mode)"; fail=1; else echo "PASS $n ($mode)"; fi
  done
  if [ -n "$GEN" ]; then $BIN --optimize --ast "$t" > "$n.astc"; fi
  if [ -f "$n.astc" ]; then
    if $BIN --optimize --ast "$t" | diff -q - "$n.astc" >/dev/null; then echo "PASS $n [astc]"; else echo "FAIL tree:    $n"; fail=1; fi
  fi
done
exit $fail
