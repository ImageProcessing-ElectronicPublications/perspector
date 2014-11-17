Perspector - a control-point-based perspective rectification tool

Description
===========

Perspector warps a picture so that the 4 pixels set as control points
become the corners of an axis-aligned rectangle.

This proves very handy to rectify rectangle-shaped objects such as building
facades.

Usage
=====

See the perspector(1) man page.

Dependencies
============

* GTK 3
* GSL

Installation
============

Run `make install` to install the program to the /usr/local prefix. You can
change the prefix from command-line, e.g.

	make install prefix='/usr'

Packagers may want to install the program to a specific folder:

	make install DESTDIR='<destdir>'

License
=======

See LICENSE.
