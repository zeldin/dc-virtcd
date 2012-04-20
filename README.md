DC-VIRTCD
=========

DC-VIRTCD is a CD virtualization system for the Dreamcast.  It allows you
to run software which accesses files on CD-ROM, without actually burning
those files on a CD-ROM.  Instead, the files are accessed over the network.
This happens transparantly, without any modifications having to be made
to the software itself.

Requirements
------------

* Broadband adapter
* A computer to run the host side of the application on
* A network cable

Features
--------

* Works with any software using Gdc system calls to read the disc
* Supports ISO and NRG images

Misfeatures
-----------

* No support for CD-DA
* Does not work with e.g. NetBSD, which accesses the GD-ROM drive directly

