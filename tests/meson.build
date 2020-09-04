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

gpg = find_program('gpg2', 'gpg')
gpgconf = find_program('gpgconf')
sign_file = [
  find_program('sign-file'),
  '--gpg-path', gpg.path(),
  '--gpgconf-path', gpgconf.path(),
  files('secret.asc'),
  '@INPUT@',
]
gz = [find_program('gzip'), '-1', '--keep', '--force', '@INPUT@']
xz = [find_program('xz'), '-0', '--keep', '--force', '@INPUT@']
make_fake_image = find_program('make-fake-image')

test_scribe_cases = {
  # "OS image" consisting of the same byte repeated to 4 MiB
  'w': '2 ** 22',
  # 
  'w-8193': '2 * 8193
}
test_scribe_generated_sources = []
foreach basename, size : test_scribe_cases
  w_img = custom_target(basename + '.img',
    command: [make_fake_image, size, '@OUTPUT@'],
    output: '@0@.img'.format(basename),
  )
  w_img_asc = custom_target(basename + '.img.asc',
    command: sign_file,
    input: w_img,
    output: 'w.img.asc',
  )
  w_img_xz = custom_target(basename + '.img.xz',
    command: xz,
    input: w_img,
    output: 'w.img.xz',
  )
  w_img_xz_asc = custom_target(basename + '.img.xz.asc',
    command: sign_file,
    input: w_img_xz,
    output: 'w.img.xz.asc',
  )
  w_img_gz = custom_target(basename + '.img.gz',
    command: gz,
    input: w_img,
    output: 'w.img.gz',
  )
  w_img_gz_asc = custom_target(basename + 'w.img.gz.asc',
    command: sign_file,
    input: w_img_gz,
    output: '@PLAINNAME@.asc',
  )
  test_scribe_generated_sources += [
    w_img,
    w_img_asc,
    w_img_xz,
    w_img_xz_asc,
    w_img_gz,
    w_img_gz_asc,
  ]
endforeach

# TODO: foo.img.?z -> foo.truncated.?z generatorw:w
# TODO: generate the three files full o' w's and then compress and sign them...

tests = {
  'dmi': {},
  'unattended-config': {},
  'write-diagnostics': {},
  # TODO: generate all the files
  'scribe': {
    'sources': [
      'test-error-input-stream.c',
      'test-error-input-stream.h',
      test_scribe_generated_sources,
    ],
    'dependencies': [
      libgisinstall_dep,
    ]
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
