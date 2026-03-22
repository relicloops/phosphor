set quiet := false
set dotenv-load := true

# Phosphor C CLI tool for NeonSignal/Cathode ecosystem
# Documentation: https://phosphor.nutsloop.host/

# Build directory
BUILD_DIR := "build"

# Phosphor version from meson.build
VERSION := "0.0.1-022"

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
  @echo "Testing recipes:"
  @echo "  test               run all unit tests (ceedling)"
  @echo "  test MODULE        run specific test module (e.g., test_kvp)"
  @echo "  integration        run integration tests"
  @echo "  coverage           run tests with coverage report"
  @echo "  coverage-open      run coverage and open HTML report"
  @echo "  coverage-docs      clobber, gcov, copy report to docs/"
  @echo ""
  @echo "Documentation & misc:"
  @echo "  docs               build Sphinx documentation"
  @echo "  changelog          regenerate CHANGELOG.md from git commits"
  @echo "  version            display phosphor version"
  @echo ""
  @echo "Complete workflows:"
  @echo "  all                clean → setup → build → test"
  @echo ""

# Configure build directory with Meson
setup:
  @echo "✓ Configuring build directory..."
  meson setup {{ BUILD_DIR }}

# Compile with Ninja
build:
  @echo "✓ Building phosphor..."
  ninja -C {{ BUILD_DIR }}

# Build and run smoke test (version check)
smoke-test: build
  @echo "✓ Running smoke test..."
  {{ BUILD_DIR }}/phosphor version

# Clean build artifacts (Ninja + Ceedling)
clean:
  @echo "✓ Cleaning build artifacts..."
  -ninja -C {{ BUILD_DIR }} clean
  -ceedling clobber

# Clean and rebuild
rebuild: clean build
  @echo "✓ Rebuild complete"

# Run all unit tests with Ceedling
test:
  @echo "✓ Running unit tests..."
  ceedling test:all

# Run specific unit test module
[no-exit-message]
test-module MODULE:
  @echo "✓ Running test module: {{ MODULE }}"
  ceedling test:{{ MODULE }}

# Run integration tests
integration: build
  @echo "✓ Running integration tests..."
  sh tests/integration/test_create_golden.sh {{ BUILD_DIR }}/phosphor

# Generate test coverage report
coverage:
  @echo "✓ Generating coverage report..."
  ceedling gcov:all
  @echo "✓ Coverage report available at:"
  @echo "  {{ BUILD_DIR }}/ceedling/artifacts/gcov/GcovCoverageResults.html"

# Generate coverage report and open in browser
coverage-open: coverage
  @echo "✓ Opening coverage report..."
  open {{ BUILD_DIR }}/ceedling/artifacts/gcov/GcovCoverageResults.html

# Clobber, regenerate coverage, and copy report to docs/source/coverage
coverage-docs:
  @echo "✓ Removing stale coverage artifacts..."
  rm -rf {{ BUILD_DIR }}/ceedling/artifacts/gcov/gcovr
  rm -rf docs/source/coverage
  @echo "✓ Cleaning ceedling state..."
  ceedling clobber
  @echo "✓ Generating coverage report..."
  ceedling gcov:all
  @echo "✓ Copying coverage to docs..."
  cp -r {{ BUILD_DIR }}/ceedling/artifacts/gcov/gcovr docs/source/coverage
  @echo "✓ Coverage report copied to:"
  @echo "  docs/source/coverage/index.html"

# Build Sphinx documentation
docs:
  @echo "✓ Building documentation..."
  sphinx-build -b html docs/source public

# Regenerate CHANGELOG.md from git commits
changelog:
  @echo "✓ Generating changelog..."
  git-cliff --config cliff.toml -o CHANGELOG.md
  @echo "✓ Changelog updated: CHANGELOG.md"

# Display phosphor version
version:
  @echo "phosphor {{ VERSION }}"

# Complete workflow: clean → setup → build → test
all: clean setup build test
  @echo "✓ All build steps completed successfully"
