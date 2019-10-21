NULL =

edit = $(AM_V_GEN) sed \
	-e 's|@SBINDIR[@]|$(sbindir)|g' \
	-e 's|@SYSCONFDIR[@]|$(sysconfdir)|g' \
	-e 's|@LIBDIR[@]|$(libdir)|g' \
	-e 's|@LIBEXECDIR[@]|$(libexecdir)|g' \
	-e 's|@LOCALSTATEDIR[@]|$(localstatedir)|g' \
	-e 's|@DATADIR[@]|$(datadir)|g'

%.desktop.in: %.desktop.in.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

rulesdir = $(datadir)/polkit-1/rules.d
rules_DATA = 90-eos-installer.rules

desktopdir = $(datadir)/applications
desktop_DATA = \
	com.endlessm.Installer.desktop \
	$(NULL)

systemdsystemunit_DATA = \
	run-mount-eosimages.mount \
	$(NULL)

icon64dir = $(datadir)/icons/hicolor/64x64/apps
icon64_DATA = \
	icons/hicolor/64x64/apps/com.endlessm.Installer.png \
	$(NULL)


EXTRA_DIST =						\
	90-eos-installer.rules  		\
	com.endlessm.Installer.desktop.in.in \
	icons/hicolor/64x64/apps/com.endlessm.Installer.png \
	run-mount-eosimages.mount
	$(NULL)

CLEANFILES =						\
	com.endlessm.Installer.desktop.in		\
	com.endlessm.Installer.desktop			\
	$(NULL)

