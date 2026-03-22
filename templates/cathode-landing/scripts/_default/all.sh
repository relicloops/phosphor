#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[>] <<name>> pipeline"

START_TIME=$(date +%s)

# Step 1: Clean
echo "[>] cleaning artifacts"
bash "$SCRIPT_DIR/clean.sh"
echo ""

# Step 2: Build
echo "[>] building site"
bash "$SCRIPT_DIR/build.sh"
echo ""

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo "[ok] pipeline complete (${ELAPSED}s)"
