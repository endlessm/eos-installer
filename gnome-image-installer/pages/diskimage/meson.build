libgisdiskimage = static_library('gisdiskimage',
    [
        gnome.compile_resources(
            'diskimage-resources',
            files('diskimage.gresource.xml'),
        ),
        'gis-diskimage-page.h',
        'gis-diskimage-page.c',
    ],
    dependencies: [
        gio_unix_dep,
        gtk_dep,
        libgiiutil_dep,
        libgisutil_dep,
        udisks_dep,
        gnome_desktop_dep,
    ],
    include_directories: [
        config_h_dir,
    ],
)
libgisdiskimage_dep = declare_dependency(
    link_with: libgisdiskimage,
    include_directories: include_directories('.'),
)
