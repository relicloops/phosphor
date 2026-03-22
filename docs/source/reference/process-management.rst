.. meta::
   :title: process management reference
   :tags: #neonsignal, #phosphor
   :status: active
   :updated: 2026-02-15

.. index::
   single: process management
   single: proc layer
   pair: api; ph_env_t
   pair: api; ph_argv_builder_t
   pair: api; ph_proc_exec

process management
==================

the proc layer provides sanitized child process execution for the ``phosphor
build`` command. it consists of three modules: environment building, argv
construction, and the execution pipeline.

headers: ``phosphor/proc.h``, ``phosphor/platform.h``

environment (ph_env_t)
----------------------

``ph_env_t`` builds a sanitized copy of the parent environment using an
allowlist strategy. only known-safe variables are passed to child processes.

types
~~~~~

.. code-block:: c

   typedef struct {
       char  **entries;    /* NULL-terminated array of "KEY=VALUE" strings */
       size_t  count;      /* number of entries (excluding NULL terminator) */
   } ph_env_t;

api
~~~

.. code-block:: c

   ph_result_t ph_env_build(const char *const extras[], ph_env_t *out);
   ph_result_t ph_env_set(ph_env_t *env, const char *key, const char *value);
   void        ph_env_destroy(ph_env_t *env);

``ph_env_build()`` filters the parent ``environ`` through:

1. **PHOSPHOR_* prefix** -- always allowed (project namespace)
2. **system allowlist** -- exact match against known-safe variables:
   ``PATH``, ``HOME``, ``USER``, ``LOGNAME``, ``LANG``, ``LC_*``, ``TERM``,
   ``SHELL``, ``TMPDIR``, ``XDG_RUNTIME_DIR``
3. **caller extras** -- additional names or prefixes. a trailing ``_`` means
   prefix match (e.g., ``"NPM_"`` allows all ``NPM_*`` variables)

``ph_env_set()`` adds or replaces a variable in the environment.

``ph_env_destroy()`` frees all heap-allocated entries and the array.

argv builder (ph_argv_builder_t)
--------------------------------

``ph_argv_builder_t`` constructs a NULL-terminated argument vector dynamically.
all strings are heap-allocated and owned by the builder until finalized.

types
~~~~~

.. code-block:: c

   typedef struct {
       char  **items;      /* NULL-terminated argv array */
       size_t  count;      /* number of arguments (excluding NULL) */
       size_t  cap;        /* allocated capacity */
   } ph_argv_builder_t;

api
~~~

.. code-block:: c

   ph_result_t  ph_argv_init(ph_argv_builder_t *b, size_t initial_cap);
   ph_result_t  ph_argv_push(ph_argv_builder_t *b, const char *arg);
   ph_result_t  ph_argv_pushf(ph_argv_builder_t *b, const char *fmt, ...);
   char       **ph_argv_finalize(ph_argv_builder_t *b);
   void         ph_argv_free(char **argv);
   void         ph_argv_destroy(ph_argv_builder_t *b);

lifecycle:

1. ``ph_argv_init()`` -- allocate with initial capacity (grows automatically)
2. ``ph_argv_push()`` / ``ph_argv_pushf()`` -- append arguments
3. ``ph_argv_finalize()`` -- transfer ownership of the argv array to caller;
   builder is invalidated
4. ``ph_argv_free()`` -- free a finalized argv array

if the builder is not finalized, use ``ph_argv_destroy()`` to clean up.

process execution (ph_proc_exec)
--------------------------------

``ph_proc_exec()`` is the high-level pipeline for spawning a child process.

types
~~~~~

.. code-block:: c

   typedef struct {
       char       **argv;
       const char  *cwd;
       ph_env_t    *env;
       int          timeout_sec;    /* reserved; ignored in v1 */
   } ph_proc_opts_t;

   typedef struct {
       int  exit_code;
       bool signaled;
       int  signal_num;
   } ph_proc_result_t;

api
~~~

.. code-block:: c

   ph_result_t ph_proc_exec(const ph_proc_opts_t *opts, int *out_exit);

pipeline:

1. extract ``envp`` from ``opts->env`` (NULL = inherit parent environment)
2. spawn child via ``ph_proc_spawn()`` (platform layer)
3. check ``ph_signal_interrupted()`` after wait (SIGINT forwarding)
4. map exit code via ``ph_proc_map_exit()``

returns ``PH_OK`` if the child was spawned and waited on (regardless of child
exit code). returns ``PH_ERR`` if the spawn itself failed. the mapped exit code
is written to ``*out_exit``.

platform layer
~~~~~~~~~~~~~~

``ph_proc_spawn()`` is implemented per-platform. the POSIX implementation
(``src/platform/posix/proc_posix.c``) uses ``fork()``/``execve()`` with:

- ``setpgid(0, 0)`` for process group isolation
- explicit ``envp`` parameter for sanitized environment
- ``SIGINT`` forwarding to the child process group

exit code mapping (ph_proc_map_exit)
------------------------------------

.. code-block:: c

   int ph_proc_map_exit(const ph_proc_result_t *result);

maps raw child exit results to phosphor exit codes (0-8):

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Child result
     - Phosphor exit
     - Meaning
   * - exit 0
     - 0
     - success
   * - exit 1
     - 1
     - general error
   * - exit 2-7
     - 2-7
     - direct pass-through
   * - exit 8+  (< 128)
     - 1
     - general/unmapped
   * - exit 128+
     - 8
     - signal (convention: 128 + signal number)
   * - signaled (no exit)
     - 8
     - killed by signal
   * - NULL result
     - 1
     - general error (defensive)

see also: :doc:`exit-codes-and-logging` for the full phosphor exit code table.
