libgisinstall = static_library('gisinstall',
    [
        gnome.compile_resources(
            'install-resources',
            files('install.gresource.xml'),
        ),
        'gis-install-page.c',
        'gis-install-page.h',
        'gis-scribe.c',
        'gis-scribe.h',
    ],
    dependencies: [
        gio_unix_dep,
        gtk_dep,
        libgiiutil_dep,
        libgisutil_dep,
        libglnx_dep,
        udisks_dep,
    ],
    include_directories: [
        config_h_dir,
    ],
)
libgisinstall_dep = declare_dependency(
    link_with: libgisinstall,
    include_directories: include_directories('.'),
)

