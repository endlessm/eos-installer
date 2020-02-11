libgisconfirm = static_library('gisconfirm',
    [
        gnome.compile_resources(
            'confirm-resources',
            files('confirm.gresource.xml'),
        ),
        'gis-confirm-page.h',
        'gis-confirm-page.c',
    ],
    dependencies: [
        gio_unix_dep,
        gtk_dep,
        libgiiutil_dep,
        libgisutil_dep,
        udisks_dep,
    ],
    include_directories: [
        config_h_dir,
    ],
)
libgisconfirm_dep = declare_dependency(
    link_with: libgisconfirm,
    include_directories: include_directories('.'),
)

