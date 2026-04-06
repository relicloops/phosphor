.. meta::
   :title: SoC audit -- header placement and JSON library consolidation
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-04-04

.. index::
   single: separation of concerns
   single: JSON parsing
   single: codebase organization
   pair: audit; SoC

SoC audit -- header placement and JSON library consolidation
=============================================================

problem
-------

after rapid feature development (phases 0-5, glow command, certs, CI pipeline),
the codebase has accumulated structural inconsistencies:

1. **header placement violation**: ``src/certs/acme_json.h`` is the only internal
   header in ``src/``. every other module exposes its API through
   ``include/phosphor/`` exclusively. this breaks the project's separation of
   concerns convention

2. **hand-rolled JSON parsing**: ``acme_json.c`` uses ``strstr()`` + manual
   character scanning to extract values from ACME protocol JSON responses. this
   approach has known limitations:

   - no escape sequence handling (``\"`` within strings breaks extraction)
   - no Unicode support beyond byte-literal matching
   - no nested object traversal
   - silent data loss on malformed JSON
   - fragile quote-counting for array extraction

   the ACME module is the only JSON consumer today, but the parsing is fragile
   enough to warrant replacement before it causes production issues

3. **naming convention**: functions ``json_extract_string()`` and
   ``json_extract_string_array()`` lack the ``ph_`` prefix required by the
   project's public API naming convention

audit findings
--------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - issue
     - severity
     - details
   * - ``acme_json.h`` in ``src/certs/``
     - convention
     - only internal header in ``src/``; all other modules use
       ``include/phosphor/`` for public API
   * - hand-rolled JSON extraction
     - correctness
     - ``strstr`` + manual quote scanning; no escape handling, no nested
       objects, silent failure on malformed input
   * - unprefixed function names
     - convention
     - ``json_extract_string`` and ``json_extract_string_array`` lack ``ph_``
       prefix; risk of symbol collision with vendored libraries
   * - ``acme_json.h`` consumers
     - scope
     - 4 files: ``acme_account.c``, ``acme_order.c``, ``acme_challenge.c``,
       ``acme_finalize.c`` -- all within ``src/certs/``

solution
--------

vendor a lightweight JSON library and consolidate all JSON parsing behind it.
move the API header to ``include/phosphor/json.h`` following codebase convention.

library candidates:

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - library
     - size
     - license
     - notes
   * - **cJSON**
     - ~2 KLOC
     - MIT
     - widely used, single .c/.h pair, DOM-style API. maintained by
       DaveGamble. already vendored in many C projects. straightforward
       integration via meson subproject
   * - **yyjson**
     - ~8 KLOC
     - MIT
     - high-performance, immutable + mutable API, SIMD-optimized.
       overkill for ACME responses but future-proof if phosphor gains
       more JSON surfaces
   * - **json.h**
     - ~1 KLOC
     - Unlicense
     - sheredom/json.h -- single header, SAX-style. minimal but limited
       API surface

**recommendation**: cJSON. it matches the project's vendoring pattern (small,
MIT-licensed, single-pair source like toml-c), has a stable API, and its
DOM-style access maps cleanly to the existing ``json_extract_string`` /
``json_extract_string_array`` call sites.

depends on
----------

- no blocking dependencies (can be done at any time)

deliverables
------------

1. cJSON vendored in ``subprojects/cjson/`` (or via ``.wrap`` file)
2. ``include/phosphor/json.h`` -- public JSON API header with ``ph_`` prefixed
   wrappers
3. ``src/core/json.c`` -- thin wrapper around cJSON (parse, extract string,
   extract string array, cleanup)
4. ACME module migrated from hand-rolled parsing to ``ph_json_*`` API
5. ``src/certs/acme_json.h`` and ``src/certs/acme_json.c`` removed
6. existing ``test_acme_json.c`` unit tests updated to use new API
7. no behavioral changes to the certs command

tasks
-----

task 1 -- vendor cJSON
^^^^^^^^^^^^^^^^^^^^^^^

add cJSON as a vendored dependency following the existing toml-c pattern.

- option A: full vendor in ``subprojects/cjson/`` with ``meson.build``
- option B: ``.wrap`` file pointing to cJSON release tarball

add ``cjson_dep`` to ``meson.build`` and link it into the phosphor binary.

acceptance criteria:

- [ ] cJSON source available in ``subprojects/``
- [ ] ``meson setup build && ninja -C build`` compiles with cJSON linked
- [ ] ``phosphor version`` smoke test passes

task 2 -- create ph_json API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

create a thin wrapper in ``include/phosphor/json.h`` + ``src/core/json.c``
that provides:

.. code-block:: c

   /* parse a JSON string into an opaque handle */
   ph_json_t *ph_json_parse(const char *json_str);

   /* extract a string value by key (top-level only) */
   char *ph_json_get_string(const ph_json_t *root, const char *key);

   /* extract a string array by key */
   char **ph_json_get_string_array(const ph_json_t *root, const char *key,
                                   size_t *out_count);

   /* free parsed JSON */
   void ph_json_destroy(ph_json_t *root);

internally delegates to cJSON. returned strings are ``ph_alloc``-ed copies
(caller frees with ``ph_free``), consistent with the existing pattern.

acceptance criteria:

- [ ] header in ``include/phosphor/json.h`` with ``ph_`` prefixed API
- [ ] implementation in ``src/core/json.c``
- [ ] handles NULL input, missing keys, type mismatches gracefully
- [ ] escape sequences and Unicode handled correctly (via cJSON)
- [ ] no ``src/`` internal headers needed

task 3 -- migrate ACME module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

replace all ``json_extract_string`` / ``json_extract_string_array`` calls in
the ACME module with the new ``ph_json_*`` API:

files to modify:

- ``src/certs/acme_account.c``
- ``src/certs/acme_order.c``
- ``src/certs/acme_challenge.c``
- ``src/certs/acme_finalize.c``

migration pattern:

.. code-block:: c

   /* before */
   char *nonce = json_extract_string(body, "newNonce");

   /* after */
   ph_json_t *json = ph_json_parse(body);
   char *nonce = ph_json_get_string(json, "newNonce");
   /* ... use nonce ... */
   ph_json_destroy(json);

acceptance criteria:

- [ ] all 4 ACME source files migrated
- [ ] no ``#include "acme_json.h"`` references remain
- [ ] ``phosphor certs --generate --dry-run`` still works
- [ ] ACME protocol flow unchanged (request/renew/verify)

task 4 -- remove old code and update tests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- delete ``src/certs/acme_json.h``
- delete ``src/certs/acme_json.c``
- remove from ``meson.build`` source list
- update ``tests/unit/test_acme_json.c`` to test ``ph_json_*`` API instead
- verify all 16 existing ACME JSON unit tests still pass (adapted)

acceptance criteria:

- [ ] ``src/certs/acme_json.h`` and ``acme_json.c`` deleted
- [ ] zero ``.h`` files remain in ``src/`` directories
- [ ] ``ceedling test:all`` passes
- [ ] ``meson setup build && ninja -C build`` clean compile

verification
------------

1. ``meson setup build && ninja -C build`` -- clean compile, no warnings
2. ``ceedling test:all`` -- all unit tests pass (including migrated JSON tests)
3. ``./build/phosphor certs --generate --dry-run`` -- certs pipeline unchanged
4. ``grep -r 'acme_json.h' src/`` -- returns zero results
5. ``find src/ -name '*.h'`` -- returns zero results
6. ``./build/phosphor version`` -- smoke test

references
----------

- ``src/certs/acme_json.h`` -- current internal header (to be removed)
- ``src/certs/acme_json.c`` -- current hand-rolled JSON extraction
- ``tests/unit/test_acme_json.c`` -- 16 existing unit tests
- ``include/phosphor/`` -- public header convention
- ``subprojects/toml-c/`` -- vendoring reference pattern
