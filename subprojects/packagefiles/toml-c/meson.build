project('toml-c', 'c',
  version : '1.0.0',
  license : 'MIT',
  default_options : ['c_std=c11', 'warning_level=2'],
)

tomlc_inc = include_directories('.')

tomlc_lib = static_library('toml-c',
  'toml.c',
  include_directories : tomlc_inc,
  install : false,
)

tomlc_dep = declare_dependency(
  include_directories : tomlc_inc,
  link_with : tomlc_lib,
)
