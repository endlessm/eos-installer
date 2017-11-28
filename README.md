Endless OS Reformatter
======================

This application is run from a live USB to write an Endless OS disk image to a
fixed disk. It is branded as “Reformat with Endless OS” or “Reformat” to avoid
confusion with the Endless Installer for Windows.

It is derived from `gnome-initial-setup`, the first boot experience for GNOME
(and Endless OS). The majority of the g-i-s code is not used.


Modes
-----

There are two supported modes of operation:

1. a combined live + installer USB, written using `eos-write-live-image` from
   `eos-meta`. This is a disk with an entire Endless OS image stored in a file
   on an `eoslive` exFAT partition. The OS is booted from that image; running
   this app from inside the image writes the image itself to a fixed disk. This
   is not very easy to replicate during development because changing the
   `eos-installer` executable invalidates the GPG signature for the image!
2. a "standalone" USB, which boots directly into this app, written using
   `eos-write-installer`. This has a normal ext4 partition holding this app,
   and a separate `eosimages` partition holding images to install. This mode is
   less useful to end users – you can't try the OS you're about to install –
   but it is an easier setup to replicate during development.


Development
-----------

One way to run this application while developing it is to use a virtual machine
with the following setup:

* Disk 1: an Endless OS installation. Build and run the app from here.
* Disk 2: a GPT-formatted drive with an `eosimages` partition. The partition
  should be exFAT-formatted, and should contain an Endless OS `.img.xz` and
  corresponding `.img.xz.asc` in the root directory. You can create such a
  drive using `eos-write-installer`.
* Disk 3: an install target. This is the drive you'll write the Endless OS image to.

If you do not have an `eosimages` partition with at least one image file on it,
running the app will take you straight to the error screen.

Spurious change
---------------
