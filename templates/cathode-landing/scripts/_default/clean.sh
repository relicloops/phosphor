#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_DIR="${PROJECT_BUILD_DIR:-$ROOT/build}"
PUBLIC_DIR="${PROJECT_PUBLIC_DIR:-$ROOT/public}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build)
      BUILD_DIR="$2"; shift 2 ;;
    --public|--deploy)
      PUBLIC_DIR="$2"; shift 2 ;;
    --help|-h)
      echo "Usage: $(basename "$0") [--build <dir>] [--public <dir>]"
      exit 0 ;;
    *)
      echo "error: unknown option: $1" >&2
      exit 1 ;;
  esac
done

echo "[>] <<name>> clean"

if [[ -d "$PUBLIC_DIR" ]]; then
  rm -rf "$PUBLIC_DIR"
  echo "[ok] removed public directory: $PUBLIC_DIR"
else
  echo "[--] public directory does not exist"
fi

if [[ -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
  echo "[ok] removed build directory: $BUILD_DIR"
else
  echo "[--] build directory does not exist"
fi

echo "[ok] clean complete"
