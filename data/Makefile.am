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

%.policy: %.policy.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

rulesdir = $(datadir)/polkit-1/rules.d
rules_DATA = 20-gnome-initial-setup.rules

actionsdir = $(datadir)/polkit-1/actions
actions_DATA = com.endlessm.TestMode.policy

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = gnome-initial-setup.session

modedir = $(datadir)/gnome-shell/modes
mode_DATA = initial-setup.json

desktopdir = $(datadir)/gdm/greeter/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	setup-shell.desktop				\
	$(NULL)

EXTRA_DIST =						\
	20-gnome-initial-setup.rules			\
	setup-shell.desktop				\
	gnome-initial-setup-first-login.desktop.in.in	\
	gnome-initial-setup.desktop.in.in		\
	gnome-initial-setup-copy-worker.desktop.in.in	\
	gnome-initial-setup.session			\
	initial-setup.json				\
	product_serial.conf                             \
	com.endlessm.TestMode.policy.in			\
	$(NULL)

CLEANFILES =						\
	gnome-initial-setup.desktop.in			\
	gnome-initial-setup.desktop			\
	gnome-initial-setup-copy-worker.desktop.in	\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	com.endlessm.TestMode.policy			\
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =					\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	$(NULL)

tmpfilesdir = $(libdir)/tmpfiles.d
tmpfiles_DATA =                                         \
	product_serial.conf                             \
	$(NULL)
