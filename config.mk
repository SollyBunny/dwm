# dwm version
VERSION = 6.5

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2
# OpenBSD (uncomment)
#FREETYPEINC = ${X11INC}/freetype2
#MANPREFIX = ${PREFIX}/man

# includes and libs
INCS = -I${X11INC} -I${FREETYPEINC} `pkg-config --cflags-only-I xft pango pangoxft`
LIBS = -L${X11LIB} -lX11 ${XINERAMALIBS} ${FREETYPELIBS} `pkg-config --libs xft pango pangoxft` -lXrender -lImlib2

# flags
DEBUGFLAGS = -O0 -g -fsanitize=address -fno-omit-frame-pointer
# CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CFLAGS   = -std=c99 -D_POSIX_C_SOURCE=200809 -pedantic -Wall -O4 ${INCS} -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${DEBUGFLAGS}
LDFLAGS  = ${LIBS} ${DEBUGFLAGS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
