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
rules_DATA = 20-eos-installer.rules

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = eos-installer.session

desktopdir = $(datadir)/gdm/greeter/applications
desktop_DATA =	eos-installer.desktop eos-setup-shell.desktop $(NULL)

EXTRA_DIST =						\
	20-eos-installer.rules  		\
	eos-installer.desktop.in.in		\
	eos-installer.session			\
	eos-setup-shell.desktop 		\
	$(NULL)

CLEANFILES =						\
	eos-installer.desktop.in		\
	eos-installer.desktop			\
	$(NULL)

