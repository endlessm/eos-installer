# Unattended mode

For mass-installation environments, `eos-installer` supports an unattended mode that can be used to reformat systems with Endless OS with minimal keypresses and other user interactions. If appropriately configured, no interaction is required beyond booting the system from USB.

At present, this mode is only supported on "standalone" reformatter USBs, which can be created using `eos-write-installer` on Endless OS, or by <kbd>Ctrl</kbd>-clicking the “click here” link at the bottom of the first page of the Endless Installer for Windows and choosing “Create Reformatter USB”.

Once you have a reformatter USB, the easiest way to configure it for unattended mode is to run it in the normal, attended mode on your target system. Once the installation is complete, press <kbd>Ctrl</kbd><kbd>U</kbd> when you reach the “Reformatting Finished” page. This will write an `unattended.ini` configuration file matching this system and the options you chose; you can now use this USB stick to reformat other computers (with the same manufacturer and model) without any user interaction.

You can also manually create an `unattended.ini` configuration file in the root of the `eosimages` partition on a reformatter USB stick, following the documentation below. All fields are optional. Just creating an empty file will cause most screens to be skipped (if possible), though confirmation will be required to begin the reformatting process. This file should be UTF-8 text, without a [byte-order mark](https://en.wikipedia.org/wiki/Byte_order_mark), conforming to the `.ini`-style format defined in the [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/latest/) and GLib's [Key-Value file parser](https://developer.gnome.org/glib/stable/glib-Key-value-file-parser.html) documentation.

## Language

By default, the reformatter user interface will be shown in _English (US)_. To present the reformatter in a different language, add a section like this to `unattended.ini`:

```ini
[EndlessOS]
locale=pt_BR.utf8
```

In the example above, the reformatter will be displayed in _Brazilian Portuguese_. You may list the available locales on an Endless OS system by running the following in a Terminal window:

```console
$ locale -a | grep utf8
```

Note that this setting has _no effect_ on the default language for the _installed_ system.

## Target Computer

To reduce the possibility of accidentally booting an unattended installer on a system with valuable data, we recommend that the disk is pre-configured with target computers. On these systems, installation will commence _without any keypresses required_, after a 30-second countdown showing what action will be taken.

These are specified in `unattended.ini` with option groups starting with `Computer`. Within each entry, `vendor` and `product` must both be specified, otherwise the entry is ignored. Here is an example targeting two computers:

```ini
[Computer 1]
vendor=Asus
product=X441SA

[Computer 2]
vendor=GIGABYTE
product=GB-BXBT-2807
```

The correct values for `vendor` and `product` for a given system can be found in `/sys/class/dmi/id/sys_vendor` and `/sys/class/dmi/id/product_name` respectively.

If no computer entries are provided, all computers will be reformatted, but the process will not begin automatically after 30 seconds: you must click a button to begin the process.  If computers are specified, using the installer on a computer that does not match any of them will show a warning, and the process will not begin automatically.

## Image

If there is only one Endless OS image file on the USB stick, and only one disk present in the system (ignoring the one the installer is running from), no configuration is needed: that image will be written to that disk. Otherwise, add an option group starting with `Image` specifying the `filename` and/or `block-device` to use:

```ini
[Image 1]
filename=eos-eos3.3-amd64-amd64.180115-104625.en.img.gz
block-device=sd
```

If specified, `block-device` can take one of two forms:

* the base disk type (for example, `sd` or `mmcblk`). In this case, eos-installer will list disks matching this name (for example, if `unattended.ini` specifies `block-device=sd`, `/dev/sda` and `/dev/sdb` would both match), ignore any which correspond to the USB device the installer is running from, and ignore any where media is not present (for example, SD card readers with no card inserted). If that leaves a single candidate, the installer can proceed unattended. Otherwise, the process fails.
* the full device path (for example, `/dev/sda`, `/dev/mmcblk0`).

At present, only writing a single image is supported; including more than one option group starting with `Image` is an error. In future, we may support specifying multiple option groups for dual-disk setups.

# `install.ini`

If you just want to set the default language of the reformatter, without triggering the unattended installation flow, create a file named `install.ini` with contents like the following:

```ini
[EndlessOS]
locale=pt_BR.utf8
```

This file is automatically created when you create a reformatter USB using the Endless Installer for Windows. If both `install.ini` and `unattended.ini` are present, and both specify a locale, `unattended.ini` takes precedence.
