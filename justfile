set quiet := false
set dotenv-load := true

# Phosphor C CLI tool for NeonSignal/Cathode ecosystem
# Documentation: https://phosphor.nutsloop.host/

# Build directory
BUILD_DIR := "build"

# Phosphor version from meson.build
VERSION := "0.0.2-023"

# Display available recipes
[default]
help:
  @echo "phosphor - C CLI tool for NeonSignal/Cathode scaffolding"
  @echo ""
  @echo "usage: just [recipe]"
  @echo ""
  @echo "Build recipes:"
  @echo "  setup              configure build directory with meson"
  @echo "  build              compile with ninja"
  @echo "  smoke-test         build and run version test"
  @echo "  clean              clean build artifacts"
  @echo "  rebuild            clean then build"
  @echo ""
  @echo "Documentation & misc:"
  @echo "  docs               build Sphinx documentation"
  @echo "  changelog          regenerate CHANGELOG.md from git commits"
  @echo "  version            display phosphor version"
  @echo ""
  @echo "Complete workflows:"
  @echo "  all                clean -> setup -> build -> smoke-test"
  @echo ""

# Configure build directory with Meson
setup:
  @echo "-- Configuring build directory..."
  meson setup {{ BUILD_DIR }}

# Compile with Ninja
build:
  @echo "-- Building phosphor..."
  ninja -C {{ BUILD_DIR }}

# Build and run smoke test (version check)
smoke-test: build
  @echo "-- Running smoke test..."
  {{ BUILD_DIR }}/phosphor version

# Clean build artifacts
clean:
  @echo "-- Cleaning build artifacts..."
  -ninja -C {{ BUILD_DIR }} clean

# Clean and rebuild
rebuild: clean build
  @echo "-- Rebuild complete"

# Build Sphinx documentation
docs:
  @echo "-- Building documentation..."
  sphinx-build -b html docs/source public

# Regenerate CHANGELOG.md from git commits
changelog:
  @echo "-- Generating changelog..."
  git-cliff --config cliff.toml -o CHANGELOG.md
  @echo "-- Changelog updated: CHANGELOG.md"

# Display phosphor version
version:
  @echo "phosphor {{ VERSION }}"

# Complete workflow: clean -> setup -> build -> smoke-test
all: clean setup build smoke-test
  @echo "-- All build steps completed successfully"
