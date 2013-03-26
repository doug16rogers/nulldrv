nulldrv Windows Device Driver
=============================

This device driver is a very simple driver that performs the following:

* Appends the local time of its installation to c:\nulldrv.sys.
* Sets its start type to SERVICE_AUTO_START.
* Does not provide a means to be unloaded.

The only way to delete it is to remove the .sys file and reboot.

The normal way that I install it is to copy it to c:\tmp then run loadnull.sh
in a git bash shell. It must then be started with 'sc start nulldrv'.

Building the Driver
-------------------

This build process has been tested using the following:

* OS: Windows 7 x64
* DDK: 7600.16385.1 (please install this)

To build, run (in a git bash shell):

  ./build.sh

See that file for more info.
