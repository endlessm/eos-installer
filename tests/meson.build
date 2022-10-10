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

# TODO: it must surely be possible to do better than this with meson
# generators
gpg = find_program('gpg2', 'gpg')
gpgconf = find_program('gpgconf')
sign_file = [
  find_program('sign-file'),
  '--gpg-path', gpg,
  '--gpgconf-path', gpgconf,
  files('secret.asc'),
  '@INPUT@',
]
gz = [find_program('gzip'), '-1', '--keep', '--force', '@INPUT@']
xz = [find_program('xz'), '-0', '--keep', '--force', '@INPUT@']
make_fake_image = find_program('make-fake-image')
cut_off_my_toes = [find_program('cut-off-my-toes'), '@INPUT@', '--']
sha256sum = [find_program('sha256sum'), '@INPUT@']

test_scribe_cases = {
  # "OS image" consisting of the same byte repeated to 4 MiB
  'w': '2 ** 22',
  # The same byte repeated to 4 MiB + 512 bytes (8193 sectors). All Endless OS
  # images are a multiple of 512 bytes, but not necessarily (in practice, almost
  # never) a multiple of 1 MiB, which is the buffer size we read and write.
  'w-8193': '512 * 8193',
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
    output: '@0@.img.asc'.format(basename),
  )
  w_img_sha256 = custom_target(basename + '.img.sha256',
    command: sha256sum,
    input: w_img,
    capture: true,
    output: '@0@.img.sha256'.format(basename),
  )
  w_img_xz = custom_target(basename + '.img.xz',
    command: xz,
    input: w_img,
    output: '@0@.img.xz'.format(basename),
  )
  w_img_xz_asc = custom_target(basename + '.img.xz.asc',
    command: sign_file,
    input: w_img_xz,
    output: '@0@.img.xz.asc'.format(basename),
  )
  w_img_xz_sha256 = custom_target(basename + '.img.xz.sha256',
    command: sha256sum,
    input: w_img_xz,
    capture: true,
    output: '@0@.img.xz.sha256'.format(basename),
  )
  w_truncated_xz = custom_target(basename + '.truncated.xz',
    command: cut_off_my_toes,
    input: w_img_xz,
    output: '@0@.truncated.xz'.format(basename),
  )
  w_truncated_xz_asc = custom_target(basename + '.truncated.xz.asc',
    command: sign_file,
    input: w_truncated_xz,
    output: '@PLAINNAME@.asc'.format(basename),
  )
  w_img_gz = custom_target(basename + '.img.gz',
    command: gz,
    input: w_img,
    output: '@0@.img.gz'.format(basename),
  )
  w_img_gz_asc = custom_target(basename + '.img.gz.asc',
    command: sign_file,
    input: w_img_gz,
    output: '@PLAINNAME@.asc',
  )
  w_img_gz_sha256 = custom_target(basename + '.img.gz.sha256',
    command: sha256sum,
    input: w_img_gz,
    capture: true,
    output: '@0@.img.gz.sha256'.format(basename),
  )
  w_truncated_gz = custom_target(basename + '.truncated.gz',
    command: cut_off_my_toes,
    input: w_img_gz,
    output: '@0@.truncated.gz'.format(basename),
  )
  w_truncated_xz_asc = custom_target(basename + '.truncated.gz.asc',
    command: sign_file,
    input: w_truncated_gz,
    output: '@PLAINNAME@.asc'.format(basename),
  )
  test_scribe_generated_sources += [
    w_img,
    w_img_asc,
    w_img_sha256,
    w_img_xz,
    w_img_xz_asc,
    w_img_xz_sha256,
    w_truncated_xz,
    w_truncated_xz_asc,
    w_img_gz,
    w_img_gz_asc,
    w_img_gz_sha256,
  ]
endforeach

tests = {
  'dmi': {},
  'unattended-config': {},
  'write-diagnostics': {},
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
    include_directories: [
      config_h_dir,
    ],
  )

  test(test_name, exe,
    env: test_env,
  )
endforeach
