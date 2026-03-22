.. meta::
   :title: embedded build toolchain -- replace esbuild
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-03-18
   :priority: high

.. index::
   single: build toolchain
   single: esbuild replacement
   single: milestone 1.0.0
   pair: embedded; bundler

embedded build toolchain -- replace esbuild [milestone 1.0.0-000]
==================================================================

.. warning::

   this plan is at the brainstorming stage. the approach, dependency selection,
   and scope are open for discussion. implementation details will solidify
   through iterative design.

problem
-------

phosphor's ``build`` command currently shells out to ``esbuild`` -- an external
Go binary that users must install separately. this creates friction:

- users need Node.js + npm/npx to get esbuild
- esbuild is a ~9 MB binary with its own release cadence
- ``phosphor doctor`` must check for esbuild availability
- the build pipeline depends on an external process (``ph_proc_exec``)
- no control over bundler behavior, error reporting, or output format

for phosphor to be a truly standalone tool ("one binary, zero runtime deps"),
the build/bundle step must be internalized.

goal
----

replace the external esbuild dependency with a build/bundle capability embedded
directly in the phosphor binary. after this milestone, ``phosphor build`` and
``phosphor glow`` produce production-ready output without requiring Node.js,
npm, or any external bundler.

this is the **v1.0.0-000 milestone** -- the point where phosphor becomes a
self-contained tool for the Cathode ecosystem.

scope
-----

what ``phosphor build`` must do today (via esbuild):

1. **JSX/TSX transpilation** -- Cathode JSX to JavaScript
2. **TypeScript transpilation** -- strip types, downlevel syntax
3. **bundling** -- resolve imports, produce single output file
4. **minification** -- compress output for production
5. **define injection** -- compile-time constants (``__PROJECT_DEV__`` etc.)
6. **static asset copying** -- CSS, HTML, images, fonts to ``public/``

the embedded toolchain must handle all six. the Cathode JSX dialect is a subset
of standard JSX (``h()`` factory, ``Fragment``, ``css()`` method) -- not the
full React/Preact surface.

approaches to evaluate
----------------------

approach A -- vendored C/C++ bundler
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

embed an existing bundler written in C or C++ as a Meson subproject, similar to
how libgit2/libarchive/pcre2 are currently vendored.

candidates:

- **custom minimal bundler**: write a purpose-built JSX/TS bundler in C that
  handles only the Cathode dialect. smallest binary impact but largest
  engineering effort. full control over error messages and behavior

- **tree-sitter + custom codegen**: use tree-sitter's C parser with a
  TypeScript/TSX grammar to parse source files into a concrete syntax tree,
  then walk the tree to emit bundled JavaScript. tree-sitter is MIT-licensed,
  mature, and designed for embedding. grammar files are separate packages

pros:

- native speed, single binary
- full control over the pipeline
- consistent with phosphor's "pure C" philosophy

cons:

- significant implementation effort
- must handle import resolution, scope analysis, minification
- TypeScript type erasure is non-trivial at full fidelity

approach B -- embedded JavaScript engine
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

embed a lightweight JavaScript engine and run a bundler (or custom transform
scripts) inside it. the bundler logic is JavaScript but runs in-process.

candidates:

- **QuickJS** (MIT, ~600 KB): embeddable ES2023 engine by Fabrice Bellard.
  small, no dependencies, compiles as a C library. could run a custom
  JSX/TS transform written in JS. fastest path to a working prototype

- **Duktape** (MIT, ~300 KB): ES5.1 engine. too old for modern JS/TS syntax

- **Hermes** (MIT, ~2 MB): Meta's engine optimized for React Native.
  good JSX support but larger footprint and C++ dependency

evaluation: QuickJS is the most promising. compile it as a Meson subproject,
write the JSX/TS transform as embedded JavaScript (similar to how template
files are embedded as C buffers), execute in-process

pros:

- leverage existing JS ecosystem knowledge for transform logic
- JSX transform is ~200 lines of JS
- QuickJS handles ES modules, async, modern syntax
- faster to implement than a custom C parser

cons:

- adds ~600 KB to binary
- JS execution overhead (acceptable for build-time tool)
- two-language codebase (C host + JS transforms)

approach C -- compile esbuild's Go WASM output
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

esbuild can compile to WebAssembly. embed a WASM runtime (e.g., wasm3, ~64 KB)
and run esbuild.wasm in-process.

pros:

- exact esbuild behavior, no reimplementation
- esbuild WASM is well-tested

cons:

- esbuild.wasm is ~8 MB (defeats the purpose of a small binary)
- WASM runtime adds complexity
- still dependent on esbuild's release cadence
- opaque error handling

**verdict**: likely too heavy. listed for completeness.

approach D -- hybrid (recommended for brainstorming)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

combine tree-sitter for parsing with a minimal custom codegen in C:

1. **parse**: tree-sitter with TypeScript/TSX grammar (CST)
2. **transform**: walk CST, handle JSX→``h()`` calls, strip TS types,
   resolve imports
3. **bundle**: concatenate transformed modules with scope wrapping
4. **minify**: basic whitespace/comment stripping (full minification deferred)
5. **define**: string replacement of ``__DEFINE__`` constants (already done
   in phosphor's renderer)

this handles the Cathode subset without needing a full JS engine. tree-sitter
grammars are maintained by the community and used by every major editor.

the question is whether import resolution and scope analysis can be kept simple
enough for the Cathode dialect. if Cathode projects follow strict conventions
(flat imports, no dynamic require, no circular deps), this is tractable.

open questions
--------------

these must be answered before selecting an approach:

architecture:

- what is the minimal subset of JSX/TS that Cathode projects actually use?
- can we constrain the Cathode dialect to make bundling simpler (e.g., no
  dynamic imports, no decorators, no enums)?
- is source map generation required for v1.0.0 or can it be deferred?
- should the bundler be a library (``libphosphor-bundle``) or tightly coupled?

dependency evaluation:

- tree-sitter C library size and meson build integration effort?
- QuickJS meson integration: how hard to vendor as a subproject?
- can tree-sitter TypeScript grammar handle Cathode JSX correctly?

performance:

- acceptable build time for a typical Cathode project (~40 files)?
- memory budget for the bundler (arena allocator integration)?

compatibility:

- must the output match esbuild byte-for-byte? (probably no -- semantic
  equivalence is sufficient)
- hot module replacement / watch mode needed for v1.0.0? (probably no)
- CSS bundling needed? (Cathode uses plain CSS files, just copy)

milestone definition
--------------------

**v1.0.0-000** is reached when:

- [ ] ``phosphor build`` produces working output without esbuild
- [ ] ``phosphor glow --name=x && cd x && phosphor build`` works end-to-end
- [ ] the phosphor binary has zero runtime dependencies on the host system
  (no Node.js, no npm, no esbuild, no Go)
- [ ] ``phosphor doctor`` no longer checks for esbuild
- [ ] existing Cathode projects (cathode-landing, website) build correctly
- [ ] binary size remains reasonable (target: under 15 MB with all deps)

non-goals for v1.0.0:

- full TypeScript type checking (``tsc`` replacement)
- React/Preact compatibility (Cathode dialect only)
- plugin system for custom transforms
- incremental/watch mode builds
- CSS preprocessing (Sass, PostCSS)
- source map generation (deferred to v1.1.0)

next steps
----------

1. audit Cathode JSX dialect: catalog every JSX/TS feature actually used
   across cathode-landing and the phosphor website
2. prototype approach B (QuickJS): vendor QuickJS, embed a 200-line JSX
   transform, benchmark against esbuild output
3. prototype approach D (tree-sitter): vendor tree-sitter + TSX grammar,
   walk CST and emit ``h()`` calls, compare output
4. compare binary size, build time, correctness, implementation effort
5. select approach and write detailed task breakdown

references
----------

- ``templates/cathode-landing/scripts/_default/build.mjs`` (current esbuild config)
- ``src/commands/build_cmd.c`` (current build command, shells out to esbuild)
- phase-4/task-2-copy-deploy-in-c (native build pipeline)
- QuickJS: https://bellard.org/quickjs/
- tree-sitter: https://tree-sitter.github.io/tree-sitter/
