# Phosphor Documentation Makefile
#
# Usage:
#   make html       - Build HTML documentation
#   make open       - Build and open in default browser
#   make clean      - Remove build artifacts
#   make livehtml   - Auto-rebuild on changes (requires sphinx-autobuild)

VENV        := .venv/bin
SPHINXBUILD := $(VENV)/sphinx-build
SOURCEDIR   := source
BUILDDIR    := build

.PHONY: help html open clean livehtml

help:
	@echo "Phosphor Documentation"
	@echo ""
	@echo "  make html       Build HTML documentation"
	@echo "  make open       Build and open in default browser"
	@echo "  make clean      Remove build artifacts"
	@echo "  make livehtml   Auto-rebuild on changes"

html:
	$(SPHINXBUILD) -b html "$(SOURCEDIR)" "$(BUILDDIR)/html"
	@echo ""
	@echo "Build finished: $(BUILDDIR)/html/index.html"

open: html
	@open "$(BUILDDIR)/html/index.html" 2>/dev/null || \
	 xdg-open "$(BUILDDIR)/html/index.html" 2>/dev/null || \
	 echo "Open $(BUILDDIR)/html/index.html in your browser"

clean:
	rm -rf "$(BUILDDIR)"

livehtml:
	$(VENV)/sphinx-autobuild "$(SOURCEDIR)" "$(BUILDDIR)/html" --open-browser
