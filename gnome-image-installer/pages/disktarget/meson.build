libgisdisktarget = static_library('gisdisktarget',
    [
        gnome.compile_resources(
            'disktarget-resources',
            files('disktarget.gresource.xml'),
        ),
        'gis-disktarget-page.h',
        'gis-disktarget-page.c',
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
libgisdisktarget_dep = declare_dependency(
    link_with: libgisdisktarget,
    include_directories: include_directories('.'),
)
