.. meta::
   :title: remote templates with YAML syntax tree
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-02-20

.. index::
   triple: phase init; task; remote YAML templates
   single: YAML syntax tree
   single: template signing
   single: package verification
   pair: remote; template
   pair: security; signing

task 4 -- remote templates with YAML syntax tree
==================================================

.. note::

   parent plan: `../masterplan.[ACTIVE ▸].rst`

.. warning::

   this task is open to reviews and changes. the YAML syntax tree schema,
   code generation strategy, and signing procedure are documented at a
   conceptual level. detailed design will evolve during implementation.

objective
---------

extend ``phosphor init`` to fetch templates from remote git repositories or
archives (reusing the libgit2/libarchive infrastructure from phase 5). remote
templates define the JSX component tree as YAML syntax tree nodes. phosphor
parses the YAML, transpiles the tree into JSX/TS/CSS source files, and writes
them interactively using the same prompt flow as the embedded template.

a vendored YAML parser library (similar to the ``toml-c`` integration pattern)
provides the parsing layer. a signing and verification procedure ensures that
remote template packages are trusted before parsing and transpilation occur.

depends on
----------

- task-3-init-command (init infrastructure in place)
- phase-5 complete (libgit2/libarchive integration)

deliverables
------------

1. YAML syntax tree schema definition for JSX component trees
2. vendored YAML parser integration (tier 1 dependency, same pattern as toml-c)
3. YAML→JSX/TS/CSS transpiler (code generation from syntax tree nodes)
4. ``--template`` flag support in init command (URL or archive path)
5. template source resolution: detect URL or archive, fetch, verify, parse YAML
6. template package signing procedure and verification
7. trust model documentation

acceptance criteria
-------------------

- [ ] ``phosphor init --name=mysite --template=https://...`` fetches remote template
- [ ] ``phosphor init --name=mysite --template=archive.tar.gz`` extracts archive
- [ ] remote template YAML syntax tree parsed into internal AST
- [ ] JSX/TS/CSS files generated from YAML tree nodes
- [ ] generated files are syntactically valid (basic structural checks)
- [ ] fallback: if remote template contains raw files (no YAML tree), treat as
      standard template with ``template.phosphor.toml``
- [ ] without libgit2/libarchive: clear error message (exit 2)
- [ ] unsigned or invalid-signature packages rejected before parsing
- [ ] signature verification reported to user before transpilation proceeds
- [ ] ``--allow-unsigned`` flag required to bypass signature check (explicit opt-in)

implementation
--------------

YAML syntax tree concept
^^^^^^^^^^^^^^^^^^^^^^^^^

remote templates represent the JSX component tree as YAML nodes rather than
shipping raw ``.tsx`` source files. phosphor reads the YAML, builds an internal
AST, and generates the corresponding source files on the fly.

conceptual example (subject to change):

.. code-block:: yaml

   # component tree definition
   tree:
     tag: App
     children:
       - tag: Header
         props:
           brand: "<<project_name>>"
         children:
           - tag: Logo
             props:
               src: "media/components/header/logo-dark.svg"
       - tag: Main
         children:
           - tag: Index
       - tag: Footer
         props:
           owner: "<<owner>>"

   # component definitions
   components:
     Header:
       file: components/layout/Header.tsx
       css: static/css/components/header.css
       props:
         brand:
           type: string
           default: "Site Name"
     Logo:
       file: components/ui/Logo.tsx
       css: static/css/components/logo.css
       props:
         src:
           type: string

the exact schema is not frozen. the above illustrates the direction: declarative
component structure → generated source files.

transpilation pipeline
^^^^^^^^^^^^^^^^^^^^^^^

1. user provides ``--template=<url|archive>``
2. init command detects remote source (URL or archive path)
3. fetch/extract to temporary directory (reuse phase-5 infrastructure)
4. **verify package signature** (see security section below)
5. detect template format:

   a. if ``template.phosphor.toml`` present → standard template (same as local)
   b. if YAML syntax tree file(s) present → transpile

6. parse YAML into internal AST
7. for each component node: generate ``.tsx``, ``.ts``, ``.css`` into staging
8. prompt user interactively for ``<<placeholder>>`` variables found in the tree
9. render placeholders into generated files
10. write to destination (same preflight/staging/commit flow as task 3)

YAML parser dependency
^^^^^^^^^^^^^^^^^^^^^^^

integrate a vendored YAML parsing library following the same pattern as
``toml-c`` (``subprojects/`` directory, meson wrap file, compile-time toggle).

candidates (to be evaluated):

- **libyaml** -- mature, MIT, event-based API. widely used (PyYAML backend).
  small footprint. good fit for the toml-c integration pattern
- **libcyaml** -- higher-level, schema-driven. depends on libyaml.
  pro: schema validation built-in. con: two dependencies instead of one
- **custom minimal parser** -- only if the YAML subset needed is small enough
  to justify avoiding a dependency

selection criteria: license (permissive), maturity, API surface, build
simplicity with meson. decision deferred to implementation time.

security: signing and verification
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

remote template packages execute a transpilation step that generates source code
from YAML definitions. this is a significant trust surface -- a malicious YAML
tree could generate harmful code that the user then builds and runs.

**signing procedure** (conceptual, subject to review):

1. template author signs the package with a private key
2. signature file (e.g., ``template.phosphor.sig``) ships alongside the YAML tree
3. phosphor verifies the signature against a known public key before parsing

**trust model options** (to be evaluated):

- **embedded public keys**: phosphor binary contains a set of trusted public keys
  (nutsloop ecosystem keys). simple but inflexible
- **keyring file**: ``~/.config/phosphor/trusted-keys.toml`` allows users to add
  trusted author keys. more flexible, user-managed
- **both**: embedded keys for official templates + user keyring for third-party.
  recommended approach

**verification flow**:

1. fetch/extract remote template
2. look for ``template.phosphor.sig`` (or similar)
3. if signature present → verify against trusted keys
4. if signature valid → proceed to parse and transpile
5. if signature missing or invalid:

   a. default: reject with error (exit 6, validation failure)
   b. with ``--allow-unsigned``: warn and proceed (user accepts risk)

6. report verification status to user before transpilation

**cryptographic library**: to be selected. candidates include libsodium
(``crypto_sign_ed25519``), openssl/libressl (already available on target
platforms), or a minimal ed25519 implementation. decision deferred.

**additional safeguards**:

- resource limits on YAML parsing (max nodes, max depth, max file size)
- generated code sandboxing is out of scope (user responsibility after init)
- ``--dry-run`` shows what would be generated without writing
- audit log of verified/unsigned templates processed

open questions
--------------

this section tracks unresolved design decisions. items will be resolved and
moved to the implementation section as the design matures.

YAML schema:

- exact node types (component, element, text, expression, fragment)
- how component props and state map to YAML fields
- how event handlers / side effects are represented
- conditional rendering (``if`` nodes) and iteration (``each`` nodes)
- slot/children composition model

code generation:

- CSS generation strategy: inline styles vs separate files vs CSS modules
- import statement generation (auto-resolve relative paths)
- TypeScript type annotation generation (props interfaces)
- how ``declare const`` globals map to YAML variable references
- Cathode-specific idioms (``h``/``Fragment`` factory, ``css()`` method)

security:

- exact signing algorithm (Ed25519 vs RSA vs ECDSA)
- key distribution and rotation policy
- revocation mechanism for compromised keys
- whether to support detached signatures or embedded signatures
- interaction with ``--allow-hooks`` (hooks in signed packages)

dependency:

- YAML parser library selection (libyaml vs libcyaml vs custom)
- cryptographic library selection
- compile-time toggles (``-Dyaml=true``, ``-Dsigning=true``)

references
----------

- phase-5/task-1-libgit2-source (git fetch infrastructure)
- phase-5/task-2-archive-support (archive extraction)
- masterplan section: dependency strategy (tier acceptance criteria)
- masterplan section: security and hardening
- masterplan section: hook security policy (related trust model)
