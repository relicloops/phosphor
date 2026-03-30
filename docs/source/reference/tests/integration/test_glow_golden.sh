#!/bin/sh
# integration test: phosphor glow -- golden output verification
#
# usage: ./tests/integration/test_glow_golden.sh [path/to/phosphor]
#
# exit 0 = all tests passed
# exit 1 = one or more tests failed

set -e

PHOSPHOR="${1:-./build/phosphor}"
TMPDIR_BASE="$(mktemp -d)"
PASS=0
FAIL=0

cleanup() {
    rm -rf "$TMPDIR_BASE"
}
trap cleanup EXIT

pass() {
    PASS=$((PASS + 1))
    printf "  [PASS] %s\n" "$1"
}

fail() {
    FAIL=$((FAIL + 1))
    printf "  [FAIL] %s\n" "$1"
}

printf "integration tests: phosphor glow\n"
printf "  binary: %s\n" "$PHOSPHOR"
printf "\n"

# ---- test 1: golden output ----
printf "test 1: golden output\n"
OUTDIR="$TMPDIR_BASE/golden"
if "$PHOSPHOR" glow --name=test-glow-golden --output="$OUTDIR" 2>/dev/null; then
    # check file count (41 embedded files produce dirs + files)
    FILE_COUNT=$(find "$OUTDIR" -type f | wc -l | tr -d ' ')
    if [ "$FILE_COUNT" -ge 30 ]; then
        pass "file count: $FILE_COUNT files created"
    else
        fail "expected >= 30 files, got $FILE_COUNT"
    fi

    # check <<name>> replaced in package.json
    if grep -q '"name": "test-glow-golden"' "$OUTDIR/package.json" 2>/dev/null; then
        pass "package.json: name replaced"
    else
        fail "package.json: <<name>> not replaced"
    fi

    # check <<name>> replaced in index.html
    if grep -q 'test-glow-golden' "$OUTDIR/src/static/index.html" 2>/dev/null; then
        pass "index.html: name replaced"
    else
        fail "index.html: <<name>> not replaced"
    fi

    # check <<name>> replaced in Header.tsx
    if grep -q 'test-glow-golden' "$OUTDIR/src/components/Header.tsx" 2>/dev/null; then
        pass "Header.tsx: name replaced"
    else
        fail "Header.tsx: <<name>> not replaced"
    fi

    # check no unreplaced <<name>> placeholders remain
    if grep -rq '<<name>>' "$OUTDIR/" 2>/dev/null; then
        fail "unreplaced <<name>> placeholders found"
    else
        pass "no unreplaced <<name>> placeholders"
    fi

    # check binary file (favicon.svg) exists and is non-empty
    if [ -s "$OUTDIR/src/static/favicon.svg" ]; then
        pass "favicon.svg exists and is non-empty"
    else
        fail "favicon.svg missing or empty"
    fi
else
    fail "glow command failed"
fi

# ---- test 2: dry-run (no files created) ----
printf "\ntest 2: dry-run\n"
DRYDIR="$TMPDIR_BASE/dryrun-output"
"$PHOSPHOR" glow --name=drytest --output="$DRYDIR" --dry-run 2>/dev/null || true
if [ ! -d "$DRYDIR" ]; then
    pass "dry-run created no output directory"
else
    fail "dry-run created output directory"
fi

# ---- test 3: force overwrite ----
printf "\ntest 3: force overwrite\n"
FORCEDIR="$TMPDIR_BASE/force-test"
"$PHOSPHOR" glow --name=force --output="$FORCEDIR" 2>/dev/null || true
# second run without --force should fail (collision)
if "$PHOSPHOR" glow --name=force --output="$FORCEDIR" 2>/dev/null; then
    fail "should have failed without --force"
else
    pass "collision detected without --force"
fi
# with --force should succeed
if "$PHOSPHOR" glow --name=force --output="$FORCEDIR" --force 2>/dev/null; then
    pass "force overwrite succeeded"
else
    fail "force overwrite failed"
fi

# ---- test 4: missing --name (exit 2) ----
printf "\ntest 4: missing --name\n"
rc=0
"$PHOSPHOR" glow 2>/dev/null || rc=$?
if [ "$rc" -eq 2 ]; then
    pass "exit code 2 for missing --name"
else
    fail "expected exit 2, got $rc"
fi

# ---- test 5: verbose output ----
printf "\ntest 5: verbose output\n"
VERBDIR="$TMPDIR_BASE/verbose-test"
VERB_OUTPUT=$("$PHOSPHOR" glow --name=verb-test --output="$VERBDIR" --verbose 2>&1 || true)
if echo "$VERB_OUTPUT" | grep -q 'glow:'; then
    pass "verbose output contains debug messages"
else
    fail "verbose output missing debug messages"
fi

# ---- summary ----
printf "\n"
printf "results: %d passed, %d failed\n" "$PASS" "$FAIL"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
