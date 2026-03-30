# -- Phosphor Documentation Configuration --
# https://www.sphinx-doc.org/en/master/usage/configuration.html

project = "Phosphor"
copyright = "2025-2026, NeonSignal Contributors"
author = "Simone Del Popolo"

# Phosphor version -- keep in sync with phosphor_version in meson.build
version = "0.0.2"
release = "0.0.2-023"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "sphinx_copybutton",
    "sphinx_design",
    "myst_parser",
]

templates_path = ["_templates"]
exclude_patterns = ["_build", ".venv", "Thumbs.db", ".DS_Store", "coverage",
                    "plans/phase-current-plan"]

# RST settings
primary_domain = "c"
highlight_language = "c"
rst_prolog = ""

# -- Options for HTML output -------------------------------------------------

html_theme = "neon-wave"
html_static_path = []
html_extra_path = ["coverage"]

html_title = "Phosphor"
html_short_title = "Phosphor"

html_theme_options = {}

# -- Extension configuration -------------------------------------------------

# sphinx-copybutton: skip prompt lines
copybutton_prompt_text = r"^\$ "
copybutton_prompt_is_regexp = True

# MyST (Markdown) support
myst_enable_extensions = [
    "colon_fence",
    "deflist",
]

# Intersphinx: link to external docs
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}
