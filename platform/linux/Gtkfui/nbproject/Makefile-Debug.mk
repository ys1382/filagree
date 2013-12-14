#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/1366059434/compile.o \
	${OBJECTDIR}/_ext/1366059434/file.o \
	${OBJECTDIR}/_ext/1366059434/hal_linux.o \
	${OBJECTDIR}/_ext/1366059434/interpret.o \
	${OBJECTDIR}/_ext/1366059434/node.o \
	${OBJECTDIR}/_ext/1366059434/serial.o \
	${OBJECTDIR}/_ext/1366059434/struct.o \
	${OBJECTDIR}/_ext/1366059434/sys.o \
	${OBJECTDIR}/_ext/1366059434/util.o \
	${OBJECTDIR}/_ext/1366059434/variable.o \
	${OBJECTDIR}/_ext/1366059434/vm.o


# C Compiler Flags
CFLAGS=-std=gnu99

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-lpthread -lm `pkg-config --libs atk` `pkg-config --libs cairo` `pkg-config --libs freetype2` `pkg-config --libs gdk-pixbuf-2.0` `pkg-config --libs gdk-3.0` `pkg-config --libs gdk-pixbuf-xlib-2.0` `pkg-config --libs glib-2.0` `pkg-config --libs gmodule-2.0` `pkg-config --libs gtk+-3.0` `pkg-config --libs gdk-x11-3.0` `pkg-config --libs gtk+-x11-3.0`  

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ../../../test/sync/gtkfui

../../../test/sync/gtkfui: ${OBJECTFILES}
	${MKDIR} -p ../../../test/sync
	${LINK.c} -o ../../../test/sync/gtkfui ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/_ext/1366059434/compile.o: ../../../source/compile.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/compile.o ../../../source/compile.c

${OBJECTDIR}/_ext/1366059434/file.o: ../../../source/file.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/file.o ../../../source/file.c

${OBJECTDIR}/_ext/1366059434/hal_linux.o: ../../../source/hal_linux.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/hal_linux.o ../../../source/hal_linux.c

${OBJECTDIR}/_ext/1366059434/interpret.o: ../../../source/interpret.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/interpret.o ../../../source/interpret.c

${OBJECTDIR}/_ext/1366059434/node.o: ../../../source/node.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/node.o ../../../source/node.c

${OBJECTDIR}/_ext/1366059434/serial.o: ../../../source/serial.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/serial.o ../../../source/serial.c

${OBJECTDIR}/_ext/1366059434/struct.o: ../../../source/struct.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/struct.o ../../../source/struct.c

${OBJECTDIR}/_ext/1366059434/sys.o: ../../../source/sys.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/sys.o ../../../source/sys.c

${OBJECTDIR}/_ext/1366059434/util.o: ../../../source/util.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/util.o ../../../source/util.c

${OBJECTDIR}/_ext/1366059434/variable.o: ../../../source/variable.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/variable.o ../../../source/variable.c

${OBJECTDIR}/_ext/1366059434/vm.o: ../../../source/vm.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DDEBUG -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/freetype2 -I/usr/include/glib-2.0 -I/usr/include/libpng12 -I/usr/include/pango-1.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/gtk-3.0 `pkg-config --cflags atk` `pkg-config --cflags cairo` `pkg-config --cflags freetype2` `pkg-config --cflags gdk-pixbuf-2.0` `pkg-config --cflags gdk-3.0` `pkg-config --cflags gdk-pixbuf-xlib-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gmodule-2.0` `pkg-config --cflags gtk+-3.0` `pkg-config --cflags gdk-x11-3.0` `pkg-config --cflags gtk+-x11-3.0`  -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/vm.o ../../../source/vm.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ../../../test/sync/gtkfui

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
