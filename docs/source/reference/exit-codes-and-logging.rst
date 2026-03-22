phosphor exit codes and logging conventions (frozen v1)
========================================================

this document is the frozen reference for phosphor exit codes, logging levels,
and parser diagnostic subcodes. phase 1 implementation must code against this
specification exactly.


part 1: exit code taxonomy
---------------------------

exit codes are stable across all phosphor commands. they form a flat taxonomy
with no overlap: every error condition maps to exactly one exit code.

.. list-table::
   :header-rows: 1
   :widths: 8 22 70

   * - code
     - category
     - description
   * - ``0``
     - success
     - command completed without error.
   * - ``1``
     - general error
     - unmapped error, including child process exit codes that do not map to a
       specific phosphor category.
   * - ``2``
     - invalid args / usage
     - cli argument parsing or semantic validation failed. all ``ux*`` diagnostic
       subcodes produce this exit code.
   * - ``3``
     - config / template parse error
     - ``template.phosphor.toml`` or project config file is syntactically or
       structurally invalid. also: malformed remote template URL, unresolvable
       git ref, or corrupted/unsupported archive file.
   * - ``4``
     - filesystem error
     - file or directory operation failed (read, write, copy, rename, delete,
       permission check).
   * - ``5``
     - process execution failure
     - child process could not be spawned, or spawned but exited with a mapped
       failure category. also: git clone/fetch failures (network, authentication,
       libgit2 initialization).
   * - ``6``
     - validation / guardrail failure
     - semantic validation of manifest, paths, or runtime constraints failed.
       this covers guardrail violations (resource limits, path safety, version
       checks). also: archive checksum mismatch, zip/tar slip prevention.
   * - ``7``
     - internal invariant violation
     - a condition that should never occur in correct code was detected.
       this indicates a phosphor bug. always accompanied by a diagnostic
       requesting a bug report.
   * - ``8``
     - interrupted by signal
     - ``SIGINT`` or ``SIGTERM`` received. cleanup was attempted before exit.


exit code triggering scenarios
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**exit 0 -- success:**

.. code-block:: text

   phosphor create --name=my-site --template=./tpl
   # project created successfully at ./my-site
   # exit 0

   phosphor version
   # phosphor 0.1.0 (abc1234 2026-02-11)
   # exit 0

   phosphor clean --dry-run
   # would remove: ./build/src, ./public/my-site.host
   # exit 0

**exit 1 -- general error:**

.. code-block:: text

   # child process exits with code 42 (no phosphor mapping)
   phosphor build --project=./broken
   # error: build script exited with code 42
   # exit 1

   # unexpected runtime failure not covered by other categories
   phosphor create --name=demo
   # error: unexpected condition during template enumeration
   # exit 1

**exit 2 -- invalid args / usage:**

.. code-block:: text

   phosphor create --naem=demo
   # error [ux001]: unknown flag '--naem'. did you mean '--name'?
   # exit 2

   phosphor build --force=true
   # error [ux002]: flag '--force' is a boolean action and does not accept a value.
   # exit 2

   phosphor create --name=demo --name=other
   # error [ux003]: duplicate flag '--name'.
   # exit 2

   phosphor create --enable-color --disable-color
   # error [ux004]: conflicting flags '--enable-color' and '--disable-color'.
   # exit 2

   phosphor create --name=123
   # error [ux005]: flag '--name' expects type 'string' matching pattern
   #   '^[a-z][a-z0-9-]{1,63}$', got '123'.
   # exit 2

   phosphor create --meta=!key:val|key:dup
   # error [ux006]: malformed kvp payload for '--meta': duplicate key 'key' at depth 0.
   # exit 2

   phosphor create --tld=.org
   # error [ux007]: flag '--tld' value '.org' is not one of: .host, .com, .io.
   # exit 2

   phosphor
   # error: no command specified. run 'phosphor help' for usage.
   # exit 2

   phosphor frobnicate
   # error: unknown command 'frobnicate'. run 'phosphor help' for usage.
   # exit 2

   # remote git template without libgit2 compiled in
   phosphor create --name=demo --template=https://github.com/user/repo
   # error: remote git templates require libgit2 support;
   #   recompile with: meson setup build -Dlibgit2=true
   # exit 2

   # archive template without libarchive compiled in
   phosphor create --name=demo --template=./template.tar.gz
   # error: archive templates require libarchive support;
   #   recompile with: meson setup build -Dlibarchive=true
   # exit 2

**exit 3 -- config / template parse error:**

.. code-block:: text

   phosphor create --name=demo --template=./bad-tpl
   # error: template.phosphor.toml: syntax error at line 14, column 3:
   #   unexpected character '}'
   # exit 3

   phosphor create --name=demo --template=./missing-schema
   # error: template.phosphor.toml: missing required key 'manifest.schema'.
   # exit 3

   phosphor build --project=./broken-config
   # error: .phosphor.toml: invalid value for 'build.deploy_at': expected path,
   #   got integer.
   # exit 3

   # malformed remote template URL
   phosphor create --name=demo --template=ftp://example.com/repo
   # error: template URL: invalid git URL scheme: ftp://example.com/repo
   #   (expected http:// or https://)
   # exit 3

   # empty ref after '#'
   phosphor create --name=demo --template=https://github.com/user/repo#
   # error: template URL: empty ref after '#' in URL
   # exit 3

   # ref not found in remote repository
   phosphor create --name=demo --template=https://github.com/user/repo#nonexistent
   # error: git fetch: cannot resolve ref 'nonexistent': reference not found
   # exit 3

   # corrupted or unsupported archive
   phosphor create --name=demo --template=./corrupted.tar.gz
   # error: archive extract: cannot open archive: ./corrupted.tar.gz
   #   (truncated gzip input)
   # exit 3

**exit 4 -- filesystem error:**

.. code-block:: text

   phosphor create --name=demo --output=/read-only-mount
   # error: cannot create directory '/read-only-mount/demo': permission denied.
   # exit 4

   phosphor build --project=./missing
   # error: project directory './missing' does not exist.
   # exit 4

   phosphor create --name=demo --template=./tpl
   # error: failed to rename staging directory to './demo': cross-device rename
   #   failed and fallback copy encountered I/O error on 'large-file.bin'.
   # exit 4

   # clone temp directory creation failure
   phosphor create --name=demo --template=https://github.com/user/repo
   # error: git fetch: cannot create clone directory: /tmp/.phosphor-clone-12345-...
   # exit 4

   # archive extraction temp directory creation failure
   phosphor create --name=demo --template=./template.tar.gz
   # error: archive extract: cannot create extraction directory:
   #   /tmp/.phosphor-extract-12345-...
   # exit 4

   # archive file not found
   phosphor create --name=demo --template=./missing.tar.gz
   # error: archive not found or not a file: /path/to/missing.tar.gz
   # exit 4

**exit 5 -- process execution failure:**

.. code-block:: text

   phosphor build --project=./my-site
   # error: failed to spawn 'esbuild': command not found.
   # exit 5

   phosphor build --project=./my-site
   # error: build script 'scripts/_default/build.sh' exited with signal SIGKILL.
   # exit 5

   # remote git clone network failure
   phosphor create --name=demo --template=https://github.com/user/private-repo
   # error: git fetch: git clone failed: authentication required but no
   #   credentials provided
   # exit 5

   # remote git clone unreachable host
   phosphor create --name=demo --template=https://unreachable.example.com/repo
   # error: git fetch: git clone failed: failed to resolve address for
   #   unreachable.example.com
   # exit 5

**exit 6 -- validation / guardrail failure:**

.. code-block:: text

   phosphor create --name=demo --template=./tpl
   # error: destination './demo' already exists. use --force to overwrite.
   # exit 6

   phosphor create --name=demo --template=./tpl
   # error: template.phosphor.toml requires phosphor >= 0.3.0, but this is 0.1.0.
   # exit 6

   phosphor create --name=../../../etc/demo
   # error: destination path escapes project root via '..'. path traversal
   #   is not allowed.
   # exit 6

   phosphor create --name=demo --template=./huge-tpl
   # error: manifest declares 2048 operations, exceeding maximum of 1024.
   # exit 6

   phosphor create --name=demo --template=./tpl
   # error: deny filter matched: '.DS_Store' in source tree. remove the file
   #   or update [filters].deny.
   # exit 6

   # archive checksum mismatch
   phosphor create --name=demo --template=./template.tar.gz \
       --checksum=sha256:0000000000000000000000000000000000000000000000000000000000000000
   # error: archive extract: checksum mismatch for /path/to/template.tar.gz:
   #   expected: 0000...0000
   #   actual:   a948...2b28
   # exit 6

   # zip/tar slip detected (path traversal in archive entry)
   phosphor create --name=demo --template=./malicious.zip
   # error: archive extract: archive entry has path traversal (zip/tar slip):
   #   ../../etc/passwd
   # exit 6

   # zip/tar slip detected (absolute path in archive entry)
   phosphor create --name=demo --template=./malicious.tar.gz
   # error: archive extract: archive entry has absolute path (zip/tar slip):
   #   /etc/passwd
   # exit 6

**exit 7 -- internal invariant violation:**

.. code-block:: text

   phosphor create --name=demo
   # INTERNAL ERROR: arena allocator double-free detected at renderer.c:142.
   # this is a bug in phosphor. please report it at:
   #   https://github.com/nutsloop/phosphor/issues
   # exit 7

   phosphor build --project=./my-site
   # INTERNAL ERROR: dispatch table missing entry for command 'build'.
   # this is a bug in phosphor. please report it at:
   #   https://github.com/nutsloop/phosphor/issues
   # exit 7

**exit 8 -- interrupted by signal:**

.. code-block:: text

   phosphor create --name=demo --template=./large-tpl
   # ^C
   # interrupted: cleaning up staging directory '.phosphor-staging-12345-1707600000'
   # exit 8

   # SIGTERM from process manager
   phosphor build --project=./my-site
   # interrupted: forwarding signal to child process group, cleaning up.
   # exit 8

   # signal during remote git clone
   phosphor create --name=demo --template=https://github.com/user/large-repo
   # ^C
   # interrupted: cleaning up clone directory '.phosphor-clone-12345-1707600000'
   # exit 8

   # signal during archive extraction
   phosphor create --name=demo --template=./large-template.tar.gz
   # ^C
   # interrupted: cleaning up extraction directory '.phosphor-extract-12345-1707600000'
   # exit 8


child process exit code mapping
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

when phosphor spawns a child process (e.g., ``esbuild``, ``build.sh``), the
child's exit code is mapped to a phosphor exit code:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - child condition
     - phosphor exit
     - rationale
   * - child exits 0
     - 0
     - success passes through
   * - child could not be spawned (ENOENT, EACCES)
     - 5
     - process execution failure
   * - child killed by signal
     - 5
     - process execution failure
   * - child exits non-zero, no specific mapping
     - 1
     - general error (unmapped)

future: command-specific child exit code mappings may be added (e.g., esbuild
exit 1 = build error → phosphor exit 5). for v1, all non-zero child exits that
are not spawn failures map to exit 1.


part 2: logging levels
-----------------------

phosphor uses five logging levels. the default level is ``info``. the
``--verbose`` flag sets the level to ``debug``. there is no user-facing flag
for ``trace`` (it is enabled via the ``PHOSPHOR_LOG=trace`` environment variable
for development only).

.. list-table::
   :header-rows: 1
   :widths: 12 18 70

   * - level
     - when to use
     - boundary rule
   * - ``error``
     - the operation cannot continue.
     - always emitted. always written to stderr. always accompanies a non-zero
       exit code. one error per failure path (no cascading error spam).
   * - ``warn``
     - the operation continues but something is degraded or unexpected.
     - emitted at ``info`` level and above. written to stderr. examples:
       EXDEV fallback during staging rename, deprecated flag usage, unknown
       keys in manifest (ignored with warning).
   * - ``info``
     - normal operational progress the user should see.
     - emitted at ``info`` level and above. written to stdout. examples:
       "creating project 'demo'", "copying 42 files", "build complete in 1.3s".
   * - ``debug``
     - detailed operational data useful for troubleshooting.
     - emitted only with ``--verbose``. written to stderr. examples:
       resolved template path, variable merge trace, individual file copy events,
       argspec resolution details.
   * - ``trace``
     - internal implementation detail for phosphor developers.
     - emitted only with ``PHOSPHOR_LOG=trace``. written to stderr. examples:
       arena allocation events, lexer token stream, parser state transitions,
       raw syscall arguments and return values.

level hierarchy and filtering
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

levels are ordered by severity: ``error > warn > info > debug > trace``.

the active log level acts as a floor: all messages at or above the active level
are emitted. messages below the active level are suppressed.

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - active level
     - messages emitted
   * - ``error``
     - error only
   * - ``warn``
     - error, warn
   * - ``info`` (default)
     - error, warn, info
   * - ``debug`` (``--verbose``)
     - error, warn, info, debug
   * - ``trace`` (``PHOSPHOR_LOG=trace``)
     - error, warn, info, debug, trace

output streams
^^^^^^^^^^^^^^^^

- **stdout**: ``info``-level messages only (operational progress).
- **stderr**: ``error``, ``warn``, ``debug``, ``trace`` messages.

rationale: stdout carries the "happy path" output that can be piped or captured.
stderr carries diagnostics that should not pollute pipeline data. this follows
the unix convention used by ``git``, ``cargo``, and ``cmake``.

output modes
^^^^^^^^^^^^^

phosphor supports three output modes, selected automatically or by flag:

1. **human** (default when stderr is a tty): colored output with unicode
   symbols, aligned columns, progress indicators.
2. **plain** (default when stderr is not a tty, or ``--no-color``): no ansi
   escape codes, no unicode decorations. suitable for CI logs.
3. **toml** (``--toml`` flag): structured toml output to stdout for machine
   consumption. logging messages still go to stderr in plain mode.

the output mode affects formatting only, never the content or exit code.


part 3: parser diagnostic subcodes
------------------------------------

all diagnostic subcodes fall under exit code ``2`` (invalid args / usage).
every diagnostic includes: flag name, expected constraint, received token,
and source token index.

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - subcode
     - category
     - description
   * - ``ux001``
     - unknown flag
     - the flag is not registered in the command's argspec table.
   * - ``ux002``
     - missing assignment / value
     - a valued flag was used without ``=value``, or a boolean flag was given
       ``=value``.
   * - ``ux003``
     - duplicate flag
     - the same flag appears more than once.
   * - ``ux004``
     - enable/disable conflict
     - both ``--enable-X`` and ``--disable-X`` appear for the same feature.
   * - ``ux005``
     - typed value mismatch
     - the token does not match the declared type for the flag.
   * - ``ux006``
     - malformed kvp payload
     - the ``!``-prefixed kvp value has structural errors.
   * - ``ux007``
     - enum choice violation
     - the value is not in the declared choices list.

diagnostic message format
^^^^^^^^^^^^^^^^^^^^^^^^^^

all diagnostics follow a consistent format:

.. code-block:: text

   error [uxNNN]: <message>.
     flag: --<flag_name>
     expected: <constraint>
     received: '<token>' (token index <N>)

example messages for each subcode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**ux001 -- unknown flag:**

.. code-block:: text

   error [ux001]: unknown flag '--naem'.
     flag: --naem
     expected: one of: --name, --template, --output, --tld, --owner, --force, ...
     received: '--naem' (token index 2)
   hint: did you mean '--name'?

**ux002 -- missing assignment / value:**

.. code-block:: text

   # valued flag without =value
   error [ux002]: flag '--name' requires a value. use '--name=<value>'.
     flag: --name
     expected: --name=<string>
     received: '--name' (token index 2)

   # boolean action with =value
   error [ux002]: flag '--force' is a boolean action and does not accept '=true'.
     flag: --force
     expected: --force (bare flag, no value)
     received: '--force=true' (token index 3)

**ux003 -- duplicate flag:**

.. code-block:: text

   error [ux003]: duplicate flag '--name'. each flag may appear at most once.
     flag: --name
     expected: single occurrence
     received: '--name=other' (token index 5)
   note: first occurrence at token index 2.

**ux004 -- enable/disable conflict:**

.. code-block:: text

   error [ux004]: conflicting flags '--enable-color' and '--disable-color'.
     flag: --enable-color / --disable-color
     expected: at most one polarity
     received: '--disable-color' (token index 4)
   note: '--enable-color' at token index 3.

**ux005 -- typed value mismatch:**

.. code-block:: text

   # string pattern mismatch
   error [ux005]: flag '--name' value '123' does not match required pattern.
     flag: --name
     expected: string matching '^[a-z][a-z0-9-]{1,63}$'
     received: '123' (token index 2)

   # integer expected, got string
   error [ux005]: flag '--port' expects an integer value.
     flag: --port
     expected: int
     received: 'abc' (token index 4)

   # url missing scheme
   error [ux005]: flag '--github' requires an explicit scheme (https:// or http://).
     flag: --github
     expected: url with scheme
     received: 'github.com/user' (token index 3)

   # path contains traversal
   error [ux005]: flag '--output' contains path traversal.
     flag: --output
     expected: path without '..' components
     received: '../escape/dir' (token index 3)

**ux006 -- malformed kvp payload:**

.. code-block:: text

   # duplicate key
   error [ux006]: malformed kvp for '--meta': duplicate key 'name' at depth 0.
     flag: --meta
     expected: unique keys at each depth
     received: '!name:a|name:b' (token index 3)

   # missing value
   error [ux006]: malformed kvp for '--meta': missing value after ':' for key 'x'.
     flag: --meta
     expected: key:value pair
     received: '!x:|y:1' (token index 3)

   # unbalanced braces
   error [ux006]: malformed kvp for '--meta': unbalanced '{' at position 8.
     flag: --meta
     expected: matching '}'
     received: '!obj:{a:1|b:2' (token index 3)

**ux007 -- enum choice violation:**

.. code-block:: text

   error [ux007]: flag '--tld' value '.org' is not a valid choice.
     flag: --tld
     expected: one of: .host, .com, .io
     received: '.org' (token index 3)


part 4: error object structure
-------------------------------

every error emitted by phosphor is internally represented as a structured error
object. this structure is used for both human-readable output and toml report
mode.

fields:

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - field
     - type
     - description
   * - ``category``
     - enum
     - maps to exit code: ``general``, ``usage``, ``config``, ``filesystem``,
       ``process``, ``validation``, ``internal``, ``signal``.
   * - ``subcode``
     - string | null
     - diagnostic subcode (e.g., ``ux003``) or null for non-parser errors.
   * - ``message``
     - string
     - human-readable error description.
   * - ``context``
     - string | null
     - path, flag name, or command that triggered the error.
   * - ``cause_id``
     - int | null
     - index into the error chain for wrapped/cascaded errors. null for
       root-cause errors.

category-to-exit-code mapping:

.. list-table::
   :header-rows: 1
   :widths: 25 15

   * - category
     - exit code
   * - ``general``
     - 1
   * - ``usage``
     - 2
   * - ``config``
     - 3
   * - ``filesystem``
     - 4
   * - ``process``
     - 5
   * - ``validation``
     - 6
   * - ``internal``
     - 7
   * - ``signal``
     - 8
