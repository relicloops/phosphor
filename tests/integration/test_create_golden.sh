#!/bin/sh
# integration test: phosphor create -- golden output + failure injection
#
# usage: ./tests/integration/test_create_golden.sh [path/to/phosphor]
#
# exit 0 = all tests passed
# exit 1 = one or more tests failed

set -e

PHOSPHOR="${1:-./build/meson/phosphor}"
FIXTURE_DIR="$(cd "$(dirname "$0")/../fixtures/minimal-template" && pwd)"
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

printf "integration tests: phosphor create\n"
printf "  binary: %s\n" "$PHOSPHOR"
printf "  fixture: %s\n" "$FIXTURE_DIR"
printf "\n"

# ---- test 1: golden output ----
printf "test 1: golden output\n"
OUTDIR="$TMPDIR_BASE/golden"
if "$PHOSPHOR" create --name=demo --template="$FIXTURE_DIR" --output="$OUTDIR" 2>/dev/null; then
    # check files exist
    if [ -f "$OUTDIR/README.md" ] && [ -f "$OUTDIR/src/main.c" ] && [ -f "$OUTDIR/include/config.h" ]; then
        pass "files created"
    else
        fail "expected files missing"
    fi

    # check rendered content
    if grep -q "# demo" "$OUTDIR/README.md" 2>/dev/null; then
        pass "README.md rendered with name"
    else
        fail "README.md not rendered correctly"
    fi

    if grep -q 'PROJECT_NAME "demo"' "$OUTDIR/include/config.h" 2>/dev/null; then
        pass "config.h rendered with name"
    else
        fail "config.h not rendered correctly"
    fi

    if grep -q 'PROJECT_AUTHOR "Test Author"' "$OUTDIR/include/config.h" 2>/dev/null; then
        pass "config.h rendered with default author"
    else
        fail "config.h default author not rendered"
    fi

    # check copy (main.c should be byte-exact copy)
    if diff -q "$FIXTURE_DIR/src/main.c" "$OUTDIR/src/main.c" >/dev/null 2>&1; then
        pass "main.c is exact copy"
    else
        fail "main.c differs from source"
    fi
else
    fail "create command failed"
fi

# ---- test 2: determinism (run twice, same output) ----
printf "\ntest 2: determinism\n"
OUTDIR2="$TMPDIR_BASE/determ"
"$PHOSPHOR" create --name=demo --template="$FIXTURE_DIR" --output="$OUTDIR2" 2>/dev/null || true
if diff -rq "$OUTDIR" "$OUTDIR2" >/dev/null 2>&1; then
    pass "output is deterministic"
else
    fail "output differs between runs"
fi

# ---- test 3: dry-run (no files created) ----
printf "\ntest 3: dry-run\n"
DRYDIR="$TMPDIR_BASE/dryrun-output"
"$PHOSPHOR" create --name=drytest --template="$FIXTURE_DIR" --output="$DRYDIR" --dry-run 2>/dev/null || true
if [ ! -d "$DRYDIR" ]; then
    pass "dry-run created no output directory"
else
    fail "dry-run created output directory"
fi

# ---- test 4: force overwrite ----
printf "\ntest 4: force overwrite\n"
FORCEDIR="$TMPDIR_BASE/force-test"
"$PHOSPHOR" create --name=force --template="$FIXTURE_DIR" --output="$FORCEDIR" 2>/dev/null || true
# second run without --force should fail (collision)
if "$PHOSPHOR" create --name=force --template="$FIXTURE_DIR" --output="$FORCEDIR" 2>/dev/null; then
    fail "should have failed without --force"
else
    pass "collision detected without --force"
fi
# with --force should succeed
if "$PHOSPHOR" create --name=force --template="$FIXTURE_DIR" --output="$FORCEDIR" --force 2>/dev/null; then
    pass "force overwrite succeeded"
else
    fail "force overwrite failed"
fi

# ---- test 5: missing --name (exit 2) ----
printf "\ntest 5: missing --name\n"
"$PHOSPHOR" create --template="$FIXTURE_DIR" 2>/dev/null || rc=$?
if [ "${rc:-0}" -eq 2 ]; then
    pass "exit code 2 for missing --name"
else
    fail "expected exit 2, got ${rc:-0}"
fi

# ---- test 6: missing template (exit 4) ----
printf "\ntest 6: missing template\n"
rc=0
"$PHOSPHOR" create --name=test --template=/tmp/nonexistent_ph_template 2>/dev/null || rc=$?
if [ "$rc" -eq 4 ]; then
    pass "exit code 4 for missing template"
else
    fail "expected exit 4, got $rc"
fi

# ---- test 7: invalid manifest (exit 3) ----
printf "\ntest 7: invalid manifest\n"
BADTMPL="$TMPDIR_BASE/bad-template"
mkdir -p "$BADTMPL"
echo "invalid = [[[broken" > "$BADTMPL/template.phosphor.toml"
rc=0
"$PHOSPHOR" create --name=test --template="$BADTMPL" 2>/dev/null || rc=$?
if [ "$rc" -eq 3 ]; then
    pass "exit code 3 for invalid manifest"
else
    fail "expected exit 3, got $rc"
fi

# ---- summary ----
printf "\n"
printf "results: %d passed, %d failed\n" "$PASS" "$FAIL"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
