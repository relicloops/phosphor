.. meta::
   :title: C buffer embedding
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-02-20

.. index::
   triple: phase init; task; C buffer embedding
   single: embedded template
   single: code generation

task 2 -- C buffer embedding
==============================

.. note::

   parent plan: `../masterplan.[ACTIVE ▸].rst`

objective
---------

convert the prepared template files into C static byte arrays compiled directly
into the phosphor binary. the ``init`` command reads template content from these
buffers -- no filesystem access needed for the embedded template.

depends on
----------

- task-1-template-preparation (all placeholders in place)

deliverables
------------

1. code generation script (or build step) that reads ``template/`` and outputs
   C source files with embedded content
2. generated C files with ``static const unsigned char[]`` arrays per template file
3. registry header mapping file paths to buffer pointers and sizes
4. meson.build integration (custom target or generated sources)
5. binary size impact documented

acceptance criteria
-------------------

- [ ] every template file has a corresponding C buffer
- [ ] buffer registry provides lookup by relative path
- [ ] binary files (SVG, favicon) embedded as raw bytes
- [ ] text files embedded preserving exact byte content
- [ ] code generation is reproducible (deterministic output)
- [ ] meson build integrates generation step
- [ ] ``phosphor version`` still works after embedding (smoke test)

implementation
--------------

approach options:

1. **build-time script**: a Python/shell script runs during ``meson`` configure
   or as a custom target, reads ``template/**``, emits ``.c`` + ``.h`` files.
   pro: automated, always in sync. con: adds build dependency.

2. **committed generated files**: run the script manually, commit the generated
   C files. pro: no build dependency. con: must re-run on template changes.

3. **xxd-style embedding**: use ``xxd -i`` or similar to generate C arrays.
   pro: simple, well-known. con: large arrays for bigger files.

generated file structure:

.. code-block:: text

   src/template/
     embedded_registry.h      -- path-to-buffer lookup API
     embedded_registry.c      -- lookup implementation
     embedded_app_tsx.c        -- static const unsigned char app_tsx[] = { ... };
     embedded_header_tsx.c     -- ...
     ...

registry API (draft):

.. code-block:: c

   typedef struct {
       const char           *path;     /* relative path, e.g. "app.tsx" */
       const unsigned char  *data;     /* buffer pointer */
       size_t                size;     /* buffer size in bytes */
       bool                  is_binary;
   } ph_embedded_file_t;

   const ph_embedded_file_t *ph_embedded_lookup(const char *path);
   size_t                    ph_embedded_count(void);
   const ph_embedded_file_t *ph_embedded_list(void);

references
----------

- masterplan section: byte-to-byte precision policy
