subdir('util')
subdir('pages')

executable('gnome-image-installer',
    [
        # FIXME: For unknown reasons, these must be compiled & linked here,
        # not linked into libgisutil.
        gis_assistant_resources,

        'gnome-image-installer.c',
        'gnome-image-installer.h',
    ],
    dependencies: [
      dependencies,
      libgisutil_dep,
      libgiiutil_dep,
      libgisconfirm_dep,
      libgisdiskimage_dep,
      libgisdisktarget_dep,
      libgisfinished_dep,
      libgisinstall_dep,
    ],
    include_directories: [
        config_h_dir,
    ],
    install: true,
    install_dir: libexecdir,
)
