gst123
======

gst123 is a command line media player for linux/unix, implemented in C++.
It is distributed under the [LGPL2](https://github.com/wenottingham/gst123/blob/master/COPYING) license.

# NOTE FOR USERS

This fork was made in order to provide some minor updates for personal use
as the upstream appears dead.

If you are looking for a general purpose, feature-rich, commandline media
player, it is **strongly** recommended that you consider an option like
[mpv](https://mpv.io), which is actively maintained by a large community.

# DESCRIPTION

The program gst123 is designed to be a more flexible command line player
in the spirit of ogg123 and mpg123, based on gstreamer. It plays all file
formats gstreamer supports, so if you have a music collection which
contains different file formats, like flac, ogg and mp3, you can use gst123
to play all your music files.

All video file formats gstreamer can play are also available.

* To submit bug reports, visit:
	https://github.com/wenottingham/gst123/issues

# REQUIREMENTS

You need a C++ compiler, gstreamer-1.0, gtk3 and ncurses to build gst123.

# INSTALLATION

In short, gst123 needs to be built and installed with:

	./configure
	make
	make install

# HISTORY

gst123 was originally developed in 2009 by Stefan Westerfeld <stefan@space.twc.de>,
and maintained and released by him through versions 0.3.5.
