project('phosphor', 'c',
  version : '0.0.2',
  license : 'Apache-2.0',
  meson_version : '>=1.1',
  default_options : [
    'c_std=c17',
    'warning_level=3',
    'buildtype=debugoptimized',
  ],
)

cc = meson.get_compiler('c')
add_project_arguments('-Wpedantic', language : 'c')

# expose POSIX.1-2008 APIs (sigaction, lstat, fnmatch, ...) under strict c17
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')

inc = include_directories('include')

# full version with build suffix (vMAJOR.MINOR.PATCH-BUILD)
phosphor_version = '0.0.2-024'

# ---- dependencies ---- #

# vendored toml-c: disable -Werror to avoid false positives in third-party code
tomlc_proj = subproject('toml-c', default_options : ['werror=false'])
tomlc_dep = tomlc_proj.get_variable('tomlc_dep')

phosphor_sources = files(
  'src/main.c',
  # cli
  'src/cli/cli_dispatch.c',
  'src/cli/cli_help.c',
  'src/cli/cli_version.c',
  # args-parser
  'src/args-parser/args_helpers.c',
  'src/args-parser/kvp.c',
  'src/args-parser/lexer.c',
  'src/args-parser/parser.c',
  'src/args-parser/spec.c',
  'src/args-parser/validate.c',
  # core
  'src/core/alloc.c',
  'src/core/bytes.c',
  'src/core/str.c',
  'src/core/vec.c',
  'src/core/arena.c',
  'src/core/error.c',
  'src/core/config.c',
  'src/core/log.c',
  'src/core/color.c',
  'src/core/term.c',
  'src/core/regex.c',
  'src/core/json.c',
  # io
  'src/io/fs_readwrite.c',
  'src/io/fs_copytree.c',
  'src/io/fs_atomic.c',
  'src/io/path_norm.c',
  'src/io/metadata_filter.c',
  'src/io/git_fetch.c',
  'src/io/archive.c',
  # crypto
  'src/crypto/sha256.c',
  # proc
  'src/proc/spawn.c',
  'src/proc/env.c',
  'src/proc/wait.c',
  # template
  'src/template/manifest_load.c',
  'src/template/var_merge.c',
  'src/template/staging.c',
  'src/template/planner.c',
  'src/template/renderer.c',
  'src/template/transform.c',
  'src/template/writer.c',
  'src/template/embedded_registry.c',
  # commands
  'src/commands/phosphor_commands.c',
  'src/commands/create_cmd.c',
  'src/commands/glow_cmd.c',
  'src/commands/build_cmd.c',
  'src/commands/clean_cmd.c',
  'src/commands/rm_cmd.c',
  'src/commands/doctor_cmd.c',
  'src/commands/certs_cmd.c',
  'src/commands/serve_cmd.c',
  'src/commands/filament_cmd.c',
  # serve
  'src/serve/serve.c',
  # dashboard
  'src/dashboard/dashboard.c',
  'src/dashboard/db_ring.c',
  'src/dashboard/db_layout.c',
  'src/dashboard/db_draw.c',
  'src/dashboard/db_popup.c',
  'src/dashboard/db_evt_pipe.c',
  'src/dashboard/db_evt_signal.c',
  'src/dashboard/db_evt_child.c',
  'src/dashboard/db_evt_key.c',
  'src/dashboard/db_evt_tick.c',
  'src/dashboard/db_event.c',
  'src/dashboard/db_lifecycle.c',
  'src/dashboard/db_fuzzy.c',
  'src/dashboard/db_json_fold.c',
  'src/dashboard/db_shell.c',
  # certs
  'src/certs/certs_config.c',
  'src/certs/certs_san.c',
  'src/certs/certs_ca.c',
  'src/certs/certs_leaf.c',
  'src/certs/acme_http.c',
  'src/certs/acme_jws.c',
  'src/certs/acme_account.c',
  'src/certs/acme_order.c',
  'src/certs/acme_challenge.c',
  'src/certs/acme_finalize.c',
  # platform
  'src/platform/signal.c',
  'src/platform/common/platform_common.c',
  'src/platform/posix/fs_posix.c',
  'src/platform/posix/proc_posix.c',
  'src/platform/posix/clock_posix.c',
)

# ---- build options ---- #

phosphor_c_args = ['-DPHOSPHOR_VERSION="' + phosphor_version + '"']

if get_option('script_fallback')
  phosphor_c_args += '-DPHOSPHOR_SCRIPT_FALLBACK=1'
endif

# ---- dependencies ---- #

cmake = import('cmake')

phosphor_deps = [tomlc_dep]

# libgit2: remote git template cloning
libgit2_opts = cmake.subproject_options()
libgit2_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS':        false,
  'BUILD_TESTS':              false,
  'BUILD_CLI':                false,
  'BUILD_EXAMPLES':           false,
  'BUILD_FUZZERS':            false,
  'USE_SSH':                  'OFF',
  'USE_BUNDLED_ZLIB':         true,
  'USE_HTTP_PARSER':          'builtin',
  'REGEX_BACKEND':            'builtin',
  'OPENSSL_USE_STATIC_LIBS':  true,
})
libgit2_opts.set_install(false)

libgit2_sub = cmake.subproject('libgit2', options : libgit2_opts)
libgit2_dep = libgit2_sub.dependency('libgit2package')

phosphor_c_args += '-DPHOSPHOR_HAS_LIBGIT2=1'
phosphor_deps += libgit2_dep

# libgit2 uses macOS frameworks and iconv that CMake resolves but the
# cmake-to-meson bridge does not propagate as link dependencies
if host_machine.system() == 'darwin'
  phosphor_deps += dependency('appleframeworks',
    modules : ['Security', 'CoreFoundation'])
  phosphor_deps += cc.find_library('iconv')
endif

# libarchive: archive template extraction (tar.gz, tar.zst, zip)
libarchive_opts = cmake.subproject_options()
libarchive_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS':     false,
  'BUILD_TESTING':         false,
  'ENABLE_TEST':           false,
  'ENABLE_TAR':            false,
  'ENABLE_CPIO':           false,
  'ENABLE_CAT':            false,
  'ENABLE_UNZIP':          false,
  'ENABLE_OPENSSL':        false,
  'ENABLE_LIBB2':          false,
  'ENABLE_LZ4':            false,
  'ENABLE_LZO':            false,
  'ENABLE_LZMA':           false,
  'ENABLE_BZIP2':          false,
  'ENABLE_ZLIB':           true,
  'ENABLE_ZSTD':           true,
  'ENABLE_LIBXML2':        false,
  'ENABLE_EXPAT':          false,
  'ENABLE_PCREPOSIX':      false,
  'ENABLE_PCRE2POSIX':     false,
  'ENABLE_ICONV':          false,
  'ENABLE_ACL':            false,
  'ENABLE_XATTR':          false,
})
libarchive_opts.set_install(false)

libarchive_sub = cmake.subproject('libarchive', options : libarchive_opts)
libarchive_dep = libarchive_sub.dependency('archive_static')

phosphor_c_args += '-DPHOSPHOR_HAS_LIBARCHIVE=1'
phosphor_deps += libarchive_dep

# libarchive may need system zlib and zstd on macOS
if host_machine.system() == 'darwin'
  phosphor_deps += cc.find_library('z')
  zstd_dep = cc.find_library('zstd', required : false)
  if zstd_dep.found()
    phosphor_deps += zstd_dep
  endif
endif

# pcre2: regex filters in template manifests
pcre2_opts = cmake.subproject_options()
pcre2_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS':        false,
  'BUILD_STATIC_LIBS':        true,
  'PCRE2_BUILD_PCRE2_8':      true,
  'PCRE2_BUILD_PCRE2_16':     false,
  'PCRE2_BUILD_PCRE2_32':     false,
  'PCRE2_BUILD_TESTS':        false,
  'PCRE2_BUILD_PCRE2GREP':    false,
  'PCRE2_SUPPORT_UNICODE':    true,
  'PCRE2_SUPPORT_JIT':        false,
})
pcre2_opts.set_install(false)

pcre2_sub = cmake.subproject('pcre2', options : pcre2_opts)
pcre2_dep = pcre2_sub.dependency('pcre2-8-static')

phosphor_c_args += '-DPHOSPHOR_HAS_PCRE2=1'
phosphor_deps += pcre2_dep

# libcurl: ACME/Let's Encrypt certificate requests
libcurl_opts = cmake.subproject_options()
libcurl_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS':        false,
  'BUILD_CURL_EXE':           false,
  'BUILD_TESTING':            false,
  'CURL_USE_OPENSSL':         true,
  'OPENSSL_USE_STATIC_LIBS':  true,
  'CURL_USE_LIBSSH2':         false,
  'CURL_DISABLE_LDAP':        true,
  'CURL_DISABLE_LDAPS':       true,
  'USE_LIBIDN2':              false,
  'CURL_DISABLE_RTSP':        true,
  'CURL_DISABLE_DICT':        true,
  'CURL_DISABLE_TELNET':      true,
  'CURL_DISABLE_TFTP':        true,
  'CURL_DISABLE_POP3':        true,
  'CURL_DISABLE_IMAP':        true,
  'CURL_DISABLE_SMTP':        true,
  'CURL_DISABLE_GOPHER':      true,
  'CURL_DISABLE_MQTT':        true,
  'HTTP_ONLY':                true,
  'CURL_USE_LIBPSL':          false,
  'CURL_BROTLI':              false,
  'CURL_ZSTD':                false,
  'USE_NGHTTP2':              false,
})
libcurl_opts.set_install(false)

libcurl_sub = cmake.subproject('curl', options : libcurl_opts)
libcurl_dep = libcurl_sub.dependency('libcurl_static')

phosphor_c_args += '-DPHOSPHOR_HAS_LIBCURL=1'
phosphor_deps += libcurl_dep

# libcurl uses macOS SystemConfiguration for proxy detection
if host_machine.system() == 'darwin'
  phosphor_deps += dependency('appleframeworks',
    modules : ['SystemConfiguration'])
endif

# OpenSSL static archives on Linux need pthread and dl
if host_machine.system() == 'linux'
  phosphor_deps += cc.find_library('pthread', required : false)
  phosphor_deps += cc.find_library('dl', required : false)
endif

# cJSON: JSON parsing for ACME + dashboard log export
cjson_opts = cmake.subproject_options()
cjson_opts.add_cmake_defines({
  'ENABLE_CJSON_TEST':     false,
  'BUILD_SHARED_LIBS':     false,
  'BUILD_SHARED_AND_STATIC_LIBS': false,
  'CJSON_BUILD_SHARED_LIBS': false,
})
cjson_opts.set_install(false)
cjson_sub = cmake.subproject('cjson', options : cjson_opts)
cjson_dep = cjson_sub.dependency('cjson')
phosphor_c_args += '-DPHOSPHOR_HAS_CJSON=1'
phosphor_deps += cjson_dep

# ncurses: serve dashboard TUI
if get_option('dashboard')
  ncurses_dep = dependency('ncurses', required : false)
  if not ncurses_dep.found()
    ncurses_dep = cc.find_library('ncurses', required : false)
  endif
  if ncurses_dep.found()
    phosphor_c_args += '-DPHOSPHOR_HAS_NCURSES=1'
    phosphor_deps += ncurses_dep
  else
    warning('ncurses not found -- serve dashboard disabled')
  endif
endif

# ---- embedded template codegen ---- #
phosphor_c_args += '-DPHOSPHOR_HAS_EMBEDDED=1'

python = find_program('python3')

# collect template files as dependencies so ninja rebuilds on changes
template_files = run_command(
  python, '-c',
  'import os, sys; d=sys.argv[1]; [print(os.path.join(r,f)) for r,_,fs in os.walk(d) for f in sorted(fs) if f != ".DS_Store" and "node_modules" not in r and "build" not in r.split(os.sep)]',
  meson.project_source_root() / 'templates' / 'cathode-landing',
  check : true,
).stdout().strip().split('\n')

embedded_data_c = custom_target('embedded_data',
  output : 'embedded_data.c',
  input : template_files,
  command : [
    python,
    meson.project_source_root() / 'scripts' / 'embed_template.py',
    meson.project_source_root() / 'templates' / 'cathode-landing',
    '@OUTPUT@',
  ],
)

phosphor_exe = executable('phosphor', phosphor_sources, embedded_data_c,
  include_directories : inc,
  dependencies : phosphor_deps,
  c_args : phosphor_c_args,
  install : true,
)
