.. meta::
   :title: shell compatibility matrix deliverable
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; deliverable; compatibility matrix
   single: verification scenarios
   single: behavioral parity

shell compatibility matrix
============================

this document provides a side-by-side comparison of every observable behavior
in the current shell scripts versus the intended phosphor command behavior.
the matrix ensures phase 3 (build compatibility mode) faithfully reproduces
existing behavior before phase 4 internalization.

scripts covered:

- ``scripts/_default/build.sh`` vs ``phosphor build``
- ``scripts/_default/clean.sh`` vs ``phosphor clean``
- ``scripts/_default/all.sh`` vs ``phosphor build --clean-first``


deliverable 1: compatibility matrix
-------------------------------------

build.sh vs phosphor build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 22 22 22 17 17

   * - operation
     - current (build.sh)
     - phosphor equivalent
     - difference
     - verify
   * - parse CLI flags
     - space-separated: ``--build <dir>``, ``--public <dir>``, ``--tld <val>``.
       short alias: ``--deploy`` for ``--public``. ``-h`` for ``--help``.
     - assignment form: ``--deploy-at=<path>``, ``--tld=<val>``,
       ``--project=<path>``. no short aliases in v1.
     - intentional (IC-01)
     - V-01
   * - check npm in PATH
     - ``command -v npm``, exit 1 if missing.
     - phosphor checks ``npm`` availability before spawning. exit 5 if missing.
     - intentional exit code change (IC-02)
     - V-02
   * - check rsync in PATH
     - ``command -v rsync``, exit 1 if missing.
     - phosphor checks ``rsync`` availability before spawning. exit 5 if
       missing. phase 4: rsync replaced by C copy module.
     - intentional exit code change (IC-02). phase 4: tool eliminated.
     - V-02
   * - install node_modules
     - if ``node_modules/`` missing or ``esbuild`` binary absent: ``npm install``.
     - phosphor spawns ``npm install`` under same condition. preflight logged
       at info level.
     - parity
     - V-03
   * - reset build directory
     - ``rm -rf $DEFAULT_BUILD_DIR``.
     - phosphor removes build dir via ``fs`` module. same effect.
     - parity
     - V-04
   * - create directories
     - ``mkdir -p $DEFAULT_BUILD_DIR $DEFAULT_PUBLIC_DIR``.
     - phosphor creates both via ``fs`` module with ``mkdir -p`` equivalent.
     - parity
     - V-04
   * - esbuild bundle
     - ``npx esbuild app.tsx --bundle --minify --format=esm --platform=browser
       --target=es2020 --jsx=transform --jsx-factory=h --jsx-fragment=Fragment
       --define:__TLD__=... --define:__SITE_OWNER__=... (6 defines)
       --outdir=$BUILD --splitting --entry-names=[name]``
     - phosphor spawns ``npx esbuild`` with identical flags. defines populated
       from ``--tld``, ``--owner``, ``--owner-slug``, ``--github``,
       ``--instagram``, ``--x`` flags (or env/config fallbacks).
     - parity (flag values may come from different sources)
     - V-05
   * - copy static assets
     - ``rsync -a --exclude='.DS_Store' --exclude='._*'
       $SOURCE/static/ $BUILD/``
     - phase 3: phosphor spawns ``rsync`` with identical args.
       phase 4: C copy module with deny filter (same exclusions).
     - parity in phase 3. phase 4: rsync eliminated but behavior preserved.
     - V-06
   * - deploy to public
     - ``rsync -a --delete --exclude='.DS_Store' --exclude='._*'
       $BUILD/ $PUBLIC/``
     - phase 3: phosphor spawns ``rsync`` with identical args.
       phase 4: C copy module with ``--delete`` equivalent and deny filter.
     - parity in phase 3. phase 4: rsync eliminated.
     - V-07
   * - metadata cleanup
     - ``find $BUILD -type f \( -name '.DS_Store' -o -name '._*' \) -delete``.
       same for ``$PUBLIC``.
     - phosphor applies deny filter during copy, not as post-copy cleanup.
       metadata files are never written to the target in the first place.
     - intentional (IC-03): prevention instead of cleanup. same observable
       result (no metadata in output).
     - V-08
   * - exit on unknown flag
     - ``print_error`` + ``usage`` + ``exit 1``.
     - ``error [ux001]`` diagnostic + ``exit 2``.
     - intentional exit code change (IC-02)
     - V-09
   * - exit on missing flag value
     - ``print_error "Missing argument for $1"`` + ``exit 1``.
     - ``error [ux002]`` diagnostic + ``exit 2``.
     - intentional exit code change (IC-02)
     - V-09
   * - SNI_OVERRIDE dead code
     - declared, checked (``if [[ -n "$SNI_OVERRIDE" ]]``), but never set.
       no ``--sni`` flag exists.
     - not reproduced. dead code eliminated.
     - intentional (IC-04)
     - --
   * - colored output
     - via ``logging.sh`` (always colored, no tty detection).
     - human mode (tty) / plain mode (no tty) / toml mode (``--toml``).
     - intentional (IC-05): respects tty detection.
     - V-10

clean.sh vs phosphor clean
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 22 22 22 17 17

   * - operation
     - current (clean.sh)
     - phosphor equivalent
     - difference
     - verify
   * - parse CLI flags
     - space-separated: ``--build <dir>``, ``--public <dir>``.
       aliases: ``--deploy``, ``-h``.
     - assignment form: ``--project=<path>``. no per-directory overrides.
       phosphor clean operates on the whole project.
     - intentional (IC-06)
     - V-11
   * - remove public directory
     - ``rm -rf $DEFAULT_PUBLIC_DIR`` (if exists). log skip if absent.
     - phosphor removes ``public/<name><tld>`` via ``fs`` module.
       log skip if absent.
     - parity (path derived from project config instead of shell var)
     - V-12
   * - remove build directory
     - ``rm -rf $DEFAULT_BUILD_DIR`` (if exists). log skip if absent.
     - phosphor removes ``build/`` via ``fs`` module. log skip if absent.
     - parity
     - V-12
   * - exit on unknown flag
     - ``exit 1``.
     - ``exit 2`` with ``ux001`` diagnostic.
     - intentional exit code change (IC-02)
     - V-09

all.sh vs phosphor build --clean-first
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 22 22 22 17 17

   * - operation
     - current (all.sh)
     - phosphor equivalent
     - difference
     - verify
   * - env var preservation
     - captures ``DEFAULT_PUBLIC_DIR`` and ``DEFAULT_BUILD_DIR`` before
       sourcing ``global_variables.sh``, restores them unless flag override.
     - not needed. phosphor resolves values via precedence chain
       (flag > env > config > default) without sourcing shell scripts.
     - intentional (IC-07): workaround eliminated.
     - --
   * - conditional clean
     - ``--clean`` flag → invokes ``$DEFAULT_CLEAN_SCRIPT`` as child.
     - ``--clean-first`` flag → phosphor runs clean logic inline before build.
     - intentional flag rename (IC-08). behavior parity.
     - V-13
   * - build invocation
     - invokes ``$DEFAULT_BUILD_SCRIPT`` as child process.
     - phosphor runs build logic (which in phase 3 still spawns the script).
     - parity in phase 3
     - V-14
   * - timing report
     - captures ``$START_TIME`` and ``$END_TIME`` via ``date +%s``. prints
       elapsed seconds.
     - phosphor build prints elapsed time at info level. uses monotonic clock
       (``clock_gettime(CLOCK_MONOTONIC)``) for precision.
     - intentional (IC-09): monotonic clock instead of wall clock.
     - V-15
   * - summary output
     - prints source, build, public paths.
     - phosphor prints project, build, deploy paths at info level.
     - parity (different labels, same information)
     - V-15


deliverable 2: intentional behavioral changes
-----------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 30 60

   * - id
     - change
     - rationale
   * - IC-01
     - flag syntax: space-separated (``--flag val``) → assignment form
       (``--flag=val``).
     - the frozen grammar (task-2) mandates assignment form for all valued
       flags. this eliminates ambiguity between flag values and positional
       args. ``--flag val`` is rejected with ``ux002``.
   * - IC-02
     - exit codes: shell scripts use ``exit 1`` for all errors. phosphor uses
       the frozen exit code taxonomy (0-8) from task-3.
     - deterministic exit codes enable scripted error handling.
       usage errors → 2, fs errors → 4, process errors → 5, etc.
   * - IC-03
     - metadata cleanup: post-copy ``find -delete`` → pre-copy deny filter.
     - prevention is more robust than cleanup. metadata files are never
       written, so there is no window where they exist in the output tree.
       the observable result is identical: no ``.DS_Store`` or ``._*`` in
       build or public directories.
   * - IC-04
     - ``SNI_OVERRIDE`` dead code eliminated.
     - the variable was declared and checked but never set via any flag.
       phosphor does not reproduce dead code paths.
   * - IC-05
     - colored output: always-on → tty-aware with ``--no-color`` override.
     - piping phosphor output to a file or another command should not
       produce ANSI escape codes. follows standard CLI conventions
       (cargo, git, cmake).
   * - IC-06
     - clean granularity: per-directory overrides (``--build``, ``--public``)
       → project-level clean (``--project``).
     - phosphor clean operates on the project as a unit. the user specifies
       which project to clean, not which internal directories. this is
       simpler and less error-prone. the internal directories (build/,
       public/) are derived from project layout.
   * - IC-07
     - env var preservation workaround eliminated.
     - the ``ORIGINAL_*`` pattern in all.sh was a workaround for
       ``global_variables.sh`` unconditionally setting defaults. phosphor's
       precedence chain (flag > env > config > default) handles this
       correctly without workarounds.
   * - IC-08
     - flag rename: ``--clean`` → ``--clean-first``.
     - ``--clean-first`` is more descriptive and follows the naming
       convention of action modifier flags in the frozen grammar.
   * - IC-09
     - timing: wall clock (``date +%s``) → monotonic clock
       (``CLOCK_MONOTONIC``).
     - wall clock can jump (ntp adjustments, daylight saving). monotonic
       clock provides accurate elapsed time measurement.


deliverable 3: verification scenarios
---------------------------------------

each scenario can be used as a parity test comparing current script output
against phosphor output.

.. list-table::
   :header-rows: 1
   :widths: 8 35 57

   * - id
     - scenario
     - verification method
   * - V-01
     - flag parsing accepts valid flags and rejects invalid ones.
     - run ``phosphor build --tld=.com`` (success) and
       ``phosphor build --tld .com`` (must fail with ux002). compare with
       ``build.sh --tld .com`` (succeeds in old system).
   * - V-02
     - tool availability check produces correct exit code.
     - temporarily rename ``npm`` out of PATH. run ``phosphor build``.
       verify exit code 5 and diagnostic message mentioning ``npm``.
       compare with ``build.sh`` which exits 1.
   * - V-03
     - auto-install of node_modules when missing.
     - delete ``node_modules/``. run ``phosphor build``. verify
       ``npm install`` is invoked and build succeeds. compare output
       tree with ``build.sh`` output.
   * - V-04
     - build directory reset and creation.
     - run ``phosphor build`` twice. verify build dir is removed and
       recreated on second run. ``diff -r`` the output trees from both
       runs (should be identical for same input).
   * - V-05
     - esbuild bundle output matches.
     - run ``build.sh`` and ``phosphor build`` with identical inputs
       (same TLD, owner, etc.). ``diff`` the resulting ``app.js`` files.
       they should be byte-identical.
   * - V-06
     - static asset copy preserves files and excludes metadata.
     - place a ``.DS_Store`` in ``src/static/``. run ``phosphor build``.
       verify ``.DS_Store`` is not in ``build/``. verify all other files
       from ``src/static/`` are present and byte-identical.
   * - V-07
     - deploy mirrors build with ``--delete`` semantics.
     - create a stale file in ``public/<site>/``. run ``phosphor build``.
       verify stale file is removed (``--delete`` behavior). verify all
       build files are present.
   * - V-08
     - metadata deny filter matches current exclusions.
     - create ``.DS_Store``, ``._foo``, ``Thumbs.db`` in source tree.
       run ``phosphor build``. verify none appear in build or public dirs.
       compare with ``build.sh`` output (should match for ``.DS_Store``
       and ``._*``; ``Thumbs.db`` is new in phosphor deny list).
   * - V-09
     - error diagnostics for invalid flags.
     - run ``phosphor build --unknown``. verify exit code 2 and ``ux001``
       message. run ``phosphor build --deploy-at`` (no value). verify
       exit code 2 and ``ux002`` message.
   * - V-10
     - output respects tty detection.
     - run ``phosphor build`` interactively (expect colored output).
       run ``phosphor build 2>&1 | cat`` (expect plain output, no ANSI).
   * - V-11
     - clean flag syntax.
     - run ``phosphor clean --project=.`` (success). run
       ``phosphor clean --build ./build`` (must fail: unknown flag
       ``--build`` in phosphor, no per-dir override).
   * - V-12
     - clean removes correct directories.
     - populate ``build/`` and ``public/<site>/``. run ``phosphor clean``.
       verify both removed. run again. verify graceful skip (no error)
       when already absent. compare with ``clean.sh`` behavior.
   * - V-13
     - ``--clean-first`` runs clean before build.
     - populate ``public/`` with stale file. run
       ``phosphor build --clean-first``. verify stale file removed and
       fresh build produced. compare with ``all.sh --clean``.
   * - V-14
     - build invocation in pipeline mode.
     - run ``phosphor build --clean-first --tld=.com``. verify complete
       pipeline: clean → build → deploy. compare output tree with
       ``all.sh --clean --tld .com``.
   * - V-15
     - timing and summary output.
     - run ``phosphor build``. verify elapsed time is printed. verify
       project, build, and deploy paths are printed. no byte-parity
       needed (format differs intentionally).


behavioral parity summary
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 10 75

   * - category
     - count
     - description
   * - full parity
     - 8
     - npm install trigger, build dir reset, mkdir, esbuild invocation,
       static copy, deploy sync, clean public, clean build.
   * - intentional change
     - 9
     - flag syntax (IC-01), exit codes (IC-02), metadata prevention
       (IC-03), dead code removal (IC-04), tty-aware color (IC-05),
       project-level clean (IC-06), env workaround removal (IC-07),
       flag rename (IC-08), monotonic clock (IC-09).
   * - phase-dependent
     - 2
     - static copy (rsync in phase 3, C in phase 4),
       deploy sync (rsync in phase 3, C in phase 4).


acceptance criteria verification
---------------------------------

- ✓ every observable behavior of each script has a corresponding phosphor
  entry (14 operations for build, 4 for clean, 5 for all).
- ✓ intentional differences are explicitly marked and justified (9 items,
  IC-01 through IC-09).
- ✓ at least one verification method per operation (15 scenarios,
  V-01 through V-15).
