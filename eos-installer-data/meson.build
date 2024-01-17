desktop_conf = configuration_data()
desktop_conf.set('LIBEXECDIR', prefix / libexecdir)

i18n.merge_file(
    input: configure_file(
        input: files('com.endlessm.Installer.desktop.in.in'),
        output: 'com.endlessm.Installer.desktop.in',
        configuration: desktop_conf
    ),
    output: 'com.endlessm.Installer.desktop',
    install_dir: join_paths(datadir, 'applications'),
    po_dir: '../po',
    install: true,
    type: 'desktop'
)

install_data(
    'run-mount-eosimages.mount',
    install_dir: systemdsystemunitdir,
)

rules_dir = join_paths(datadir, 'polkit-1', 'rules.d')
install_data(
    '90-eos-installer.rules',
    install_dir: rules_dir,
)

icon_dir = 'icons/hicolor/scalable/apps'
install_data(
    icon_dir / 'com.endlessm.Installer.svg',
    install_dir: datadir / icon_dir,
)

symbolic_icon_dir = 'icons/hicolor/symbolic/apps'
install_data(
    symbolic_icon_dir / 'com.endlessm.Installer-symbolic.svg',
    install_dir: datadir / symbolic_icon_dir,
)
