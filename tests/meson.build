test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('MALLOC_CHECK_', '2')
# Using GFile causes GIOModules to be probed, which (if gvfs is installed)
# ultimately causes g_socket_class_init() to be called, which (as documented)
# causes SIGPIPE to be ignored for the remainder of the program. This masked a
# test failure on development systems (where gvfs is installed) which was found
# on the build server (gvfs is not listed as a dependency, so SIGPIPE was not
# implicitly ignored).
#
# Setting the search path to the empty string causes no modules to be loaded.
test_env.set('GIO_MODULE_DIR', '')

tests = {
  'dmi': {},
  'unattended-config': {},
  'write-diagnostics': {},
  # TODO: generate all the files
  'scribe': {
    'sources': [
      'test-error-input-stream.c',
      'test-error-input-stream.h',
    ],
    'dependencies': [libgisinstall_dep],
  },
}

foreach test_name, test_options : tests
  deps = [
    gio_unix_dep,
    libglnx_dep,
    libgiiutil_dep,
  ] + test_options.get('dependencies', [])
  sources = ['test-' + test_name + '.c'] + test_options.get('sources', [])
  exe = executable(test_name,
    sources,
    dependencies: deps,
  )

  test(test_name, exe,
    env: test_env,
  )
endforeach
