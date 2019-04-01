About
-----

This fork of bbswitch is a kernel module which automatically takes control
over your dedicated Nvidia GPU and keeps it off.
This solves the full system hangs caused by reading the PCI config using sysfs,
which wake up the device. The hangs can be seen by running `lspci` or by
letting the device sleep.
I wasn't able to solve this issue using any combination of bumblebee,
optimus-manager, and bbswitch.

Use at your own risk.

Build
-----

Build the module (kernel headers are required):

    make
Then install it (requires root privileges, i.e. `sudo`):

    make install

DKMS support
------------

If you have DKMS installed, you can install bbswitch in such a way that it
survives kernel upgrades. It is recommended to remove older versions of bbswitch
by running `dkms remove -m bbswitch -v OLDVERSION --all` as root. To install
the new version, simply run:

    # make -f Makefile.dkms

To uninstall it, run:

    # make -f Makefile.dkms uninstall
