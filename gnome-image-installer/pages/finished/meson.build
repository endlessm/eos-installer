libgisfinished = static_library('gisfinished',
    [
        gnome.compile_resources(
            'finished-resources',
            files('finished.gresource.xml'),
        ),
        'gis-finished-page.h',
        'gis-finished-page.c',
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
libgisfinished_dep = declare_dependency(
    link_with: libgisfinished,
    include_directories: include_directories('.'),
)
