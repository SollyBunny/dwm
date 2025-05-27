# dwm version
VERSION = 6.5-solly

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

CC = cc

NODRW = 0 # Set to 1 to disable drawing

INCS = 
LIBS = 
FLAG = 

# X11
LIBS += -I/usr/X11R6/lib -lX11
INCS += -L/usr/X11R6/include

# Xinerama
LIBS += -lXinerama
FLAG += -DXINERAMA

ifeq ($(strip $(NODRW)), 0)

# freetype
LIBS += -lfontconfig -lXft
INCS += -I/usr/include/freetype2
# OpenBSD (uncomment)
# LIBS += ${X11INC}/freetype2
# incs += ${PREFIX}/man

# Imlib2
LIBS += -lXrender -lImlib2

else

FLAG += -DNODRW

endif

FLAG += -Wall -pedantic -Wextra

# Debug Info
FLAG += -g
# Debug Flags
# FLAG += -O1 -fsanitize=address -fno-omit-frame-pointer
# Release Flags
FLAG += -O4
# Solaris Relase Flags (uncomment)
# FLAG += -fast

# flags
CFLAGS   = -std=c99 -D_POSIX_C_SOURCE=200809 -pedantic -Wall -DVERSION=\"${VERSION}\" ${INCS} ${FLAG}
LDFLAGS  = ${LIBS} ${FLAG}

