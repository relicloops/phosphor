
template.phosphor.toml -- manifest schema
==========================================

schema version 1. all template directories must contain a
``template.phosphor.toml`` file at the root.

[manifest] -- metadata
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``schema``
     - integer
     - yes
     - Schema version (must be ``1``).
   * - ``id``
     - string
     - yes
     - Template slug identifier (e.g. ``neonsignal-landing``).
   * - ``version``
     - string
     - yes
     - Semver version of the template (e.g. ``0.1.0``).

[template] -- template metadata
--------------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``name``
     - string
     - no
     - Human-readable template name.
   * - ``source_root``
     - string
     - yes
     - Relative path to the template source directory.
   * - ``description``
     - string
     - no
     - One-line description.
   * - ``min_phosphor``
     - string
     - no
     - Minimum phosphor version required (semver). If the running CLI is
       older, exit code 6 is returned.
   * - ``license``
     - string
     - no
     - License identifier (SPDX).

[[variables]] -- variable definitions
--------------------------------------

Array of tables. each entry defines a template variable.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``name``
     - string
     - yes
     - Variable name (referenced as ``<<name>>`` in templates).
   * - ``type``
     - string
     - no
     - One of: ``string``, ``bool``, ``int``, ``enum``, ``path``, ``url``.
       Default: ``string``.
   * - ``required``
     - boolean
     - no
     - If true, the variable must be resolved at merge time. Default: false.
   * - ``default``
     - string
     - no
     - Default value (lowest precedence in merge chain).
   * - ``env``
     - string
     - no
     - Environment variable name to read (precedence level 2).
   * - ``pattern``
     - string
     - no
     - POSIX extended regex the resolved value must match.
   * - ``min``
     - integer
     - no
     - Minimum value (type ``int`` only).
   * - ``max``
     - integer
     - no
     - Maximum value (type ``int`` only).
   * - ``choices``
     - array of strings
     - no
     - Valid values (type ``enum`` only).
   * - ``secret``
     - boolean
     - no
     - If true, value is masked in logs. Default: false.

[filters] -- file filtering
----------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``exclude``
     - array of strings
     - no
     - Glob patterns to exclude from processing.
   * - ``deny``
     - array of strings
     - no
     - Hard-deny patterns (error if matched).
   * - ``binary_ext``
     - array of strings
     - no
     - Extensions treated as binary (copied, not rendered).
   * - ``text_ext``
     - array of strings
     - no
     - Extensions treated as text (rendered).

[build] -- build configuration
-------------------------------

Optional. declares build-time defines and entry point for ``phosphor build``.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``entry``
     - string
     - no
     - Entry point for esbuild (e.g. ``"src/app.tsx"``). Default: ``src/app.tsx``.

[[build.defines]] -- build-time defines
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Array of tables. each entry declares a build-time define injected via
``--define:NAME="VALUE"`` into esbuild.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``name``
     - string
     - yes
     - Define name (e.g. ``__PHOSPHOR_DEV__``).
   * - ``env``
     - string
     - no
     - Environment variable to read value from at build time.
   * - ``default``
     - string
     - no
     - Fallback value if ``env`` is unset or empty.

[deploy] -- deploy configuration
---------------------------------

Optional. declares the deploy target directory for ``phosphor build`` and
provides derived defaults for ``phosphor serve``.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``public_dir``
     - string
     - no
     - Relative deploy path (e.g. ``"public/mysite.host"``). Used by build
       command tier-2 resolution and by serve command to derive ``--www-root``
       via ``dirname(public_dir)``.

[serve] -- serve configuration
-------------------------------

Optional. declares defaults for ``phosphor serve``. all values are overridden
by CLI flags when present. the serve command resolves each setting in order:
CLI flag > ``[serve]`` manifest > ``[deploy]``/``[certs]`` derived > built-in
default.

Top-level keys:

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``no_redirect``
     - boolean
     - no
     - Skip launching the HTTP->HTTPS redirect service. Default: false.

[serve.neonsignal] -- neonsignal process defaults
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``bin``
     - string
     - no
     - Path to neonsignal binary. Default: search PATH.
   * - ``threads``
     - integer
     - no
     - Worker thread count. Default: 3.
   * - ``host``
     - string
     - no
     - Bind address. Default: ``0.0.0.0``.
   * - ``port``
     - integer
     - no
     - HTTPS listen port. Default: 9443.
   * - ``www_root``
     - string
     - no
     - Static files root. Default: derived from ``[deploy]`` or ``"public"``.
   * - ``certs_root``
     - string
     - no
     - TLS certs directory. Default: from ``[certs].output_dir`` or ``"certs"``.
   * - ``working_dir``
     - string
     - no
     - Working directory for path resolution.
   * - ``upload_dir``
     - string
     - no
     - Upload directory override.
   * - ``augments_dir``
     - string
     - no
     - API augments directory.
   * - ``grafts_dir``
     - string
     - no
     - Grafts directory.
   * - ``watch``
     - boolean
     - no
     - Start file watcher alongside server. Default: false.
   * - ``watch_cmd``
     - string
     - no
     - Shell command for watch process. Default:
       ``node scripts/_default/build.mjs --watch``.

[serve.redirect] -- redirect process defaults
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``bin``
     - string
     - no
     - Path to neonsignal_redirect binary. Default: search PATH.
   * - ``instances``
     - integer
     - no
     - Worker instances. Default: 2.
   * - ``host``
     - string
     - no
     - Bind address. Default: ``0.0.0.0``.
   * - ``port``
     - integer
     - no
     - HTTP listen port. Default: 9090.
   * - ``target_port``
     - integer
     - no
     - HTTPS target port for redirection. Default: neonsignal port.
   * - ``acme_webroot``
     - string
     - no
     - ACME webroot directory for HTTP-01 challenges.
   * - ``working_dir``
     - string
     - no
     - Working directory for path resolution.

[[ops]] -- operations
---------------------

Array of tables. at least one entry is required.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``id``
     - string
     - no
     - Unique identifier for this operation.
   * - ``kind``
     - string
     - yes
     - One of: ``mkdir``, ``copy``, ``render``, ``chmod``, ``remove``.
   * - ``from``
     - string
     - conditional
     - Source path (required for ``copy``, ``render``, ``chmod``).
   * - ``to``
     - string
     - conditional
     - Destination path (required for ``copy``, ``render``, ``mkdir``).
   * - ``mode``
     - string
     - no
     - Octal permission string (e.g. ``"0755"``). For ``chmod`` and ``mkdir``.
   * - ``overwrite``
     - boolean
     - no
     - Allow overwriting existing files. Default: false.
   * - ``condition``
     - string
     - no
     - Expression evaluated at plan time. Operation is skipped if false.
   * - ``atomic``
     - boolean
     - no
     - Use atomic write (temp + fsync + rename). Default: false.
   * - ``newline``
     - string
     - no
     - Newline normalization: ``lf``, ``crlf``, or ``keep``. Default: ``keep``.

[[hooks]] -- lifecycle hooks
-----------------------------

Array of tables. executed before/after template operations.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``when``
     - string
     - yes
     - Hook timing: ``pre-create`` or ``post-create``.
   * - ``run``
     - array of strings
     - yes
     - Command argv (e.g. ``["git", "init"]``).
   * - ``cwd``
     - string
     - no
     - Working directory for the command.
   * - ``condition``
     - string
     - no
     - Expression; hook is skipped if false.
   * - ``allow_failure``
     - boolean
     - no
     - If true, hook failure does not abort the pipeline. Default: false.

[defaults] -- key-value defaults
---------------------------------

Flat table of key-value pairs used as fallback values in the merge pipeline
(precedence level 4).

[certs] -- TLS certificate configuration
-----------------------------------------

Optional. declares certificate generation settings for ``phosphor certs``.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``output_dir``
     - string
     - no
     - Output directory for generated certificates. Default: ``"certs"``.
   * - ``ca_cn``
     - string
     - no
     - Common Name for the local root CA. Default: ``"phosphor-local-CA"``.
   * - ``ca_bits``
     - integer
     - no
     - RSA key size for the CA key. Default: ``4096``.
   * - ``ca_days``
     - integer
     - no
     - Validity period for the CA certificate in days. Default: ``3650``.
   * - ``leaf_bits``
     - integer
     - no
     - RSA key size for leaf certificate keys. Default: ``2048``.
   * - ``leaf_days``
     - integer
     - no
     - Validity period for leaf certificates in days. Default: ``825``.
   * - ``account_key``
     - string
     - no
     - Path to the ACME account key. Default: ``~/.phosphor/acme/account.key``.

[[certs.domains]] -- domain entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Array of tables. each entry defines a domain for certificate generation.

.. list-table::
   :header-rows: 1
   :widths: 20 10 10 60

   * - Key
     - Type
     - Required
     - Description
   * - ``name``
     - string
     - yes
     - Primary domain name (e.g. ``"example.com"``).
   * - ``mode``
     - string
     - yes
     - Certificate mode: ``"local"`` (self-signed CA) or ``"letsencrypt"``
       (ACME HTTP-01).
   * - ``san``
     - array of strings
     - no
     - Subject Alternative Names. Supports DNS names and IP addresses.
   * - ``dir_name``
     - string
     - no
     - Override the output subdirectory name. Default: the domain ``name``.
   * - ``email``
     - string
     - conditional
     - Contact email for Let's Encrypt registration. Required when
       ``mode = "letsencrypt"``.
   * - ``webroot``
     - string
     - conditional
     - Path to the ACME challenge webroot directory. Required when
       ``mode = "letsencrypt"``. Must match the challenge-serving HTTP
       server's document root.

resource limits
---------------

- ``PH_MAX_OPS``: 1024
- ``PH_MAX_VARIABLES``: 128
- ``PH_MAX_BUILD_DEFINES``: 64
- ``PH_MAX_HOOKS``: 16
- ``PH_MAX_DIR_DEPTH``: 32
- ``PH_MAX_CERT_DOMAINS``: 64

example manifest
-----------------

.. code-block:: toml

   [manifest]
   schema = 1
   id = "neonsignal-landing"
   version = "0.1.0"

   [template]
   name = "NeonSignal Landing Page"
   source_root = "src"
   description = "Static landing site template for NeonSignal"
   min_phosphor = "0.1.0"
   license = "Apache-2.0"

   [[variables]]
   name = "project_name"
   type = "string"
   required = true

   [[variables]]
   name = "tld"
   type = "enum"
   default = ".host"
   choices = [".host", ".com", ".io"]

   [filters]
   exclude = ["*.bak", ".git"]
   binary_ext = [".png", ".jpg", ".gif", ".ico", ".woff2"]

   [build]
   entry = "src/app.tsx"

   [[build.defines]]
   name = "__PROJECT_DEV__"
   env = "PROJECT_DEV"
   default = "false"

   [[build.defines]]
   name = "__PROJECT_PUBLIC_DIR__"
   env = "PROJECT_PUBLIC_DIR"
   default = ""

   [deploy]
   public_dir = "public/<<project_name>>.host"

   [serve]
   no_redirect = false

   [serve.neonsignal]
   port = 9443
   www_root = "public"
   certs_root = "certs"

   [serve.redirect]
   port = 9090
   target_port = 9443

   [[ops]]
   id = "create-root"
   kind = "mkdir"
   to = "<<project_name>>"

   [[ops]]
   id = "copy-sources"
   kind = "render"
   from = "src"
   to = "<<project_name>>/src"

   [[hooks]]
   when = "post-create"
   run = ["git", "init"]
   cwd = "<<project_name>>"
   allow_failure = true

   [certs]
   output_dir = "certs"
   ca_cn = "phosphor-local-CA"

   [[certs.domains]]
   name = "example.host"
   mode = "local"
   san = ["example.host", "10.0.0.10"]

   [[certs.domains]]
   name = "example.com"
   mode = "letsencrypt"
   san = ["example.com", "www.example.com"]
   email = "admin@example.com"
   webroot = "/var/www/acme-challenge"
