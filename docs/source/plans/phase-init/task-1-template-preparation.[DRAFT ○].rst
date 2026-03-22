.. meta::
   :title: template file preparation
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-02-20

.. index::
   triple: phase init; task; template preparation
   single: placeholder substitution
   single: template variables

task 1 -- template file preparation
=====================================

.. note::

   parent plan: `../masterplan.[ACTIVE ▸].rst`

objective
---------

edit all 46 template files in ``template/`` to replace hardcoded site-specific
values with ``<<placeholder>>`` syntax. create a ``template.phosphor.toml``
manifest that defines the variables, operations, and filters for the embedded
template. this is prerequisite work before C buffer embedding.

depends on
----------

- template files copied to ``template/`` (done)

deliverables
------------

1. all build-time globals replaced with ``<<var>>`` placeholders
2. site-specific content genericized for reuse
3. ``template/template.phosphor.toml`` manifest created
4. variable-to-placeholder mapping documented

acceptance criteria
-------------------

- [ ] ``__TLD__`` usage replaced with ``<<tld>>`` in all .tsx/.ts files
- [ ] ``__SITE_GITHUB__`` replaced with ``<<github>>``
- [ ] ``__SITE_INSTAGRAM__`` replaced with ``<<instagram>>``
- [ ] ``__SITE_X__`` replaced with ``<<x>>``
- [ ] ``__SITE_OWNER__`` replaced with ``<<owner>>``
- [ ] ``declare const`` blocks for globals removed or replaced
- [ ] fallback patterns (``typeof __X__ !== 'undefined' ? __X__ : default``)
      simplified to direct ``<<var>>`` references
- [ ] HTML files use ``<<project_name>>`` in titles and meta tags
- [ ] ``template.phosphor.toml`` defines all variables with correct types
- [ ] ``template.phosphor.toml`` defines ``[[ops]]`` for the full directory tree
- [ ] ``template.phosphor.toml`` defines ``[filters]`` for binary vs text extensions

implementation
--------------

variable mapping:

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - build-time global
     - placeholder
     - type
     - default
   * - ``__TLD__``
     - ``<<tld>>``
     - enum
     - ``.host``
   * - ``__SITE_GITHUB__``
     - ``<<github>>``
     - url
     - (empty)
   * - ``__SITE_INSTAGRAM__``
     - ``<<instagram>>``
     - url
     - (empty)
   * - ``__SITE_X__``
     - ``<<x>>``
     - url
     - (empty)
   * - ``__SITE_OWNER__``
     - ``<<owner>>``
     - string
     - ``Site Owner``
   * - (new)
     - ``<<project_name>>``
     - string
     - (required, no default)

files requiring edits (by category):

- **TSX components** (16 files): replace ``declare const`` + ternary fallback
  patterns with direct ``<<var>>`` usage
- **HTML templates** (2 files): ``index.html``, ``notfound.html`` -- title, meta
- **CSS files** (20 files): likely no variable substitution needed (pure styles)
- **SVG files** (3 files): ``favicon.svg``, ``logo-dark.svg``, ``logo-light.svg``
  -- check for embedded text/colors
- **Config** (1 file): ``.cathode`` routing hints

references
----------

- masterplan section: configuration and template model
- docs/source/reference/template-manifest-schema.rst
