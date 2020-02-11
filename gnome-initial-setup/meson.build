gis_assistant_resources = gnome.compile_resources(
    'gis-assistant-resources',
    files('gis-assistant.gresource.xml'),
)

libgisutil = static_library('gisutil',
    [
        gis_assistant_resources,
        'gis-assistant.c',
        'gis-assistant.h',
        'gis-driver-mode.c',
        'gis-driver-mode.h',
        'gis-driver.c',
        'gis-driver.h',
        'gis-page.c',
        'gis-page.h',
        'gis-window.c',
        'gis-window.h',
        'gnome-initial-setup.h',
    ],
    dependencies: [
        gtk_dep,
    ],
    include_directories: [
        config_h_dir,
    ]
)
libgisutil_dep = declare_dependency(
    link_with: libgisutil,
    include_directories: include_directories('.'),
)
