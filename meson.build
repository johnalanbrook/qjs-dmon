project('qjs-dmon', 'c')

quickjs = dependency('quickjs', static:true)

inc = include_directories('.')

dmon_lib = library('dmon',
  'qjs_dmon.c',
  include_directories:inc,
  dependencies: [quickjs],
  c_shared_args: '-DJS_SHARED_LIBRARY',
)

qjs_dmon_dep = declare_dependency(
  link_with: dmon_lib,
  include_directories:inc,  
)
