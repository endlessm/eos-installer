project('eos-installer',
        ['c'],
        version: '3.35.91',
        license: 'GPLv2',
        meson_version: '>= 0.47.0',
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-std=gnu99', language: 'c')

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

libglnx_dep = subproject('libglnx').get_variable('libglnx_dep')
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
  dependency('x11'),
]

systemd_dep = dependency('systemd')
systemdsystemunitdir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')

gpg = find_program('gpg')
gpgconf = find_program('gpgconf')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR',  join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('LOCALSTATEDIR',   join_paths(get_option('prefix'), get_option('localstatedir')))
conf.set_quoted('DATADIR',         join_paths(get_option('prefix'), get_option('datadir')))
conf.set_quoted('GPG_PATH',        gpg.path())

configure_file(output: 'config.h', configuration: conf)
config_h_dir = include_directories('.')

subdir('gnome-initial-setup')
subdir('gnome-image-installer')
#subdir('po')
#subdir('eos-installer-data')
subdir('tests')
