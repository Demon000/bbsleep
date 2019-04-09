About
-----

bbsleep is a fork of bbswitch which automatically takes control
over your dedicated Nvidia GPU and keeps it off.
This solves the full system hangs caused by reading the PCI config using sysfs,
which wake up the device. The hangs can be seen by running `lspci` or by
letting the device sleep.

Use at your own risk.

Build
-----

Build the module (kernel headers are required):

    make
Then install it (requires root privileges, i.e. `sudo`):

    make install

DKMS support
------------

If you have DKMS installed, you can install bbsleep in such a way that it
survives kernel upgrades. It is recommended to remove older versions of bbsleep
by running `dkms remove -m bbsleep -v OLDVERSION --all` as root. To install
the new version, simply run:

    # make -f Makefile.dkms

To uninstall it, run:

    # make -f Makefile.dkms uninstall
