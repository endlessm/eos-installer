project('eos-installer',
        ['c'],
        version: '3.35.91',
        license: 'GPLv2',
        meson_version: '>= 0.55.0',
        default_options: [
            'c_std=gnu99',
            'warning_level=2',
        ],
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

libglnx_dep = subproject(
    'libglnx',
    # https://gitlab.gnome.org/GNOME/libglnx/-/merge_requests/36
    default_options: ['warning_level=2'],
).get_variable('libglnx_dep')
gio_unix_dep = dependency('gio-unix-2.0', version: '>= 2.40.0')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.7.11')
udisks_dep = dependency('udisks2')
gnome_desktop_dep = dependency('gnome-desktop-3.0', version: '>= 3.7.5')

dependencies = [
  gio_unix_dep,
  gtk_dep,
  udisks_dep,
  gnome_desktop_dep,
  dependency('liblzma'),
]

prefix = get_option('prefix')
libexecdir = get_option('libexecdir')
datadir = get_option('datadir')

systemd_dep = dependency('systemd')
systemdsystemunitdir = systemd_dep.get_variable(
    pkgconfig: 'systemdsystemunitdir',
    pkgconfig_define: ['prefix', prefix],
)

gpg = find_program('gpg')
gpgconf = find_program('gpgconf')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR',  join_paths(prefix, get_option('localedir')))
conf.set_quoted('LOCALSTATEDIR',   join_paths(prefix, get_option('localstatedir')))
conf.set_quoted('DATADIR',         join_paths(prefix, datadir))
conf.set_quoted('GPG_PATH',        gpg.full_path())

configure_file(output: 'config.h', configuration: conf)
config_h_dir = include_directories('.')

subdir('gnome-initial-setup')
subdir('gnome-image-installer')
subdir('po')
subdir('eos-installer-data')
subdir('tests')
