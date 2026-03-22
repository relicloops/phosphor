#!/usr/bin/env bash
# Delegates to build.mjs (esbuild context API for incremental builds)
exec node "$(dirname "$0")/build.mjs" "$@"
