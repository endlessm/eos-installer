Endless OS Reformatter
======================

![](./eos-installer-data/icons/hicolor/64x64/apps/com.endlessm.Installer.png)

This application is run from a live USB to write an Endless OS disk image to a
fixed disk. It is branded as “Reformat with Endless OS” or “Reformat” to avoid
confusion with the [Endless Installer for Windows][rufus]. While primarily
designed for Endless OS, it may be useful elsewhere — for example, [GNOME OS
Nightly](https://os.gnome.org/) ISOs use a modified version of `eos-installer` —
and we welcome patches to make it more generic.

The codebase was originally derived from [`gnome-initial-setup`][gis], the
first boot experience for GNOME (and Endless OS), and this is still visible in
file and symbol names, but the majority of the `gnome-initial-setup` code has
been removed, leaving only the skeleton needed for this app.

It is typically launched by [our branch of
`gnome-initial-setup`][endlessm-gis], though can be launched within a user
session as a normal app in some scenarios, as described below.

Unlike a traditional installer, this application knows almost nothing about the
OS being installed. It takes a disk image (possibly compressed) and either a GPG
signature or a SHA256 checksum, and writes the disk image to disk while
verifying its GPG signature or checksum in parallel. The disk image is treated
as just a stream of bytes, except in these respects:

* The start of the image is examined to check that it has a [GUID Partition
  Table](https://en.wikipedia.org/wiki/GUID_Partition_Table) with an [EFI
  system partition](https://en.wikipedia.org/wiki/EFI_system_partition) and a
  Linux root filesystem with flag 55 set, which is true of all Endless OS
  (and GNOME OS) disk images.
* The uncompressed size of the image is determined from the GPT to provide
  accurate progress reporting and verify that the target drive is large enough.
  While most supported compression formats (XZ, SquashFS) provide this
  information, gzip only provides the uncompressed size modulo 2<sup>32</sup>,
  and Endless OS disk images are typically (much!) larger than this threshold.
* The first 1 MiB of the disk image is reserved, with zeros written to disk
  instead. Once the rest of the image has been decompressed and written, if its
  GPG signature or SHA256 checksum is valid, then the first 1MiB is written at the start of the
  disk. This prevents booting into a partially-written or corrupt installation.

Apart from these details, you can essentially think of this app as a glorified
GUI for `zcat | gpg --verify | dd`.

The GPG signature, if present, is verified against a keyring located at
`/usr/share/keyrings/eos-image-keyring.gpg`. If both a GPG signature and SHA256
checksum are present, the GPG signature is preferred.

[rufus]: https://github.com/endlessm/rufus
[gis]: https://gitlab.gnome.org/gnome/gnome-initial-setup
[endlessm-gis]: https://github.com/endlessm/gnome-initial-setup


Installing from Live Image
--------------------------

When booted into an Endless OS live system, eos-installer can be used to write
a copy of the running (but unmodified) OS image to permanent storage.  To allow
this, eos-installer is shipped as part of the normal Endless OS ostree, but is
hidden from the end user unless the OS is booted in live mode.

During the `gnome-initial-setup` flow, the user is offered the choice to try a
live session, or run the reformatter immediately. If they choose the former,
this application can be launched as a normal app from the live session.

There are two disk layouts for live systems:

1. A large exFAT partition with label `eoslive`, which contains an uncompressed
   Endless OS image file and associated signature, next to the normal EFI and
   BIOS boot partitions. We sometimes refer to this layout as a *combined live
   & installer USB*. It's created using [`eos-write-live-image`][ewli]. 
2. An ISO image, produced by [eos-image-builder][eib] as part of the OS build
   process. Ignoring the bootloader stuff, this is an ISO9660 filesystem which
   contains a SquashFS disk image containing an Endless OS disk image, and
   signatures or checksums for both the uncompressed disk image and the SquashFS image
   alongside it.

These are both treated very similarly by `eos-installer`. Once it has located
the partition containing the GPG signature for the uncompressed image, it reads
the disk image itself from the read-only device-mapper device that the system
is booted from. This avoids `eos-installer` needing to know how to extract a
file from a SquashFS image. It's also convenient during development because you
can basically ignore the SquashFS case.

Because the `eos-installer` application is run from *within the disk image
being installed*, this configuration is quite hard to test during development:
replacing the `eos-installer` binary modifies the disk image and so invalidates
its GPG signature.

[ewli]: https://github.com/endlessm/eos-meta/blob/master/eos-tech-support/eos-write-live-image
[eib]: http://github.com/endlessm/eos-image-builder


Installing with Standalone Image
--------------------------------

We also publish an `eosinstaller` ostree & OS image. This is a cut-down build
of the OS which excludes almost all the normal applications: it can only be
used to run eos-installer (this application) from the `gnome-initial-setup`
greeter session.

[`eos-write-installer`][ewi] writes an `eosinstaller` disk image plus an OS
image to be installed to removable media. The resulting partition layout is:

- EFI and BIOS boot partitions
- a small ext4 partition containing the `eosinstaller` OS deployment
- an exFAT partition with label `eosimages`, occupying the rest of the device
  containing the OS image to be installed and its corresponding signature or checksum
  
One can copy additional OS images and their corresponding signatures or checksums to the
exFAT partition, and they'll be offered as extra choices by `eos-installer`.

This mode is less useful to end users – you can't try the OS you're about to
install – but it is an easier setup to replicate during development. (In fact,
`eos-installer` doesn't check that the `eosimages` partition is on the same
physical device that it's running from.)

This is the only mode which supports [unattended installation](./UNATTENDED.md).

[ewi]: https://github.com/endlessm/eos-meta/blob/master/eos-tech-support/eos-write-installer

Development
-----------

One way to run this application while developing it is with the following setup:

* Host system: a normal Endless OS installation. Build `eos-installer` in
  `toolbox`, and run it on the host system.
* Disk 1: a GPT-formatted drive with an exFAT partition with label `eosimages`.
  As shown below, this can be a loopback device if you want to avoid using
  removable media, but it has to have a GPT.  This partition should contain, in
  its root directory:
  - a GPT disk image (`.img`, `.img.xz` or
    `.img.gz`). `xz` decompression is *really* slow, so `gz` is strongly
    recommended.
  - either a corresponding `.img(.[gx]z)?.asc` GPG
  signature, or a corresponding `.img(.[gx]z)?.sha256` SHA-256 checksum.
* Disk 2: a target disk or loop associated file large enough to write the OS
  image to. `eos-installer` only considers non-removable disks with a
  corresponding block device to be install targets, so unless you have a
  computer with multiple built-in disks, you'll need to either do all this in a
  virtual machine with multiple fixed disks, or use a loopback device.

To use loop devices for testing, the following procedure can be used:

```
# Create a source disk image to store the images to be installed
truncate -s 5G src.img
# Write a GPT partition table with a single Linux partition
sfdisk src.img <<"EOF"
label: gpt
type=L
EOF
# Attach a loop device with partition scanning to the source image
src_loop=$(sudo losetup -P --show -f src.img)
# Make an exFAT file system on the first partition with the eosimages label
sudo mkfs.exfat -n eosimages "${src_loop}p1"
# Mount it and copy the to be installed image files to it
sudo mount -t exfat -o "uid=$(id -u),gid=$(id -g)" "${src_loop}p1" /mnt
cp eos*.img.* /mnt
# Create a target disk image to install to
truncate -s 5G tgt.img
# Attach a loop device with partition scanning to the target image
tgt_loop=$(sudo losetup -P --show -f tgt.img)
```

Now `eos-installer` can be run. To cleanup, unmount the source image and detach
the loop devices:

```
sudo umount /mnt
sudo losetup -d "$src_loop"
sudo losetup -d "$tgt_loop"
```

If you do not have an `eosimages` partition with at least one image file on it,
running the app will take you straight to the error screen.
