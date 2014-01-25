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
CND_PLATFORM=Cygwin_4.x-Windows
CND_DLIB_EXT=dll
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
	${OBJECTDIR}/_ext/1366059434/hal_windows.o \
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
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ../../../test/sync/filagree.exe

../../../test/sync/filagree.exe: ${OBJECTFILES}
	${MKDIR} -p ../../../test/sync
	${LINK.c} -o ../../../test/sync/filagree ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/_ext/1366059434/compile.o: ../../../source/compile.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/compile.o ../../../source/compile.c

${OBJECTDIR}/_ext/1366059434/file.o: ../../../source/file.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/file.o ../../../source/file.c

${OBJECTDIR}/_ext/1366059434/hal_windows.o: ../../../source/hal_windows.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/hal_windows.o ../../../source/hal_windows.c

${OBJECTDIR}/_ext/1366059434/interpret.o: ../../../source/interpret.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/interpret.o ../../../source/interpret.c

${OBJECTDIR}/_ext/1366059434/node.o: ../../../source/node.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/node.o ../../../source/node.c

${OBJECTDIR}/_ext/1366059434/serial.o: ../../../source/serial.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/serial.o ../../../source/serial.c

${OBJECTDIR}/_ext/1366059434/struct.o: ../../../source/struct.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/struct.o ../../../source/struct.c

${OBJECTDIR}/_ext/1366059434/sys.o: ../../../source/sys.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/sys.o ../../../source/sys.c

${OBJECTDIR}/_ext/1366059434/util.o: ../../../source/util.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/util.o ../../../source/util.c

${OBJECTDIR}/_ext/1366059434/variable.o: ../../../source/variable.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/variable.o ../../../source/variable.c

${OBJECTDIR}/_ext/1366059434/vm.o: ../../../source/vm.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1366059434
	${RM} $@.d
	$(COMPILE.c) -g -DFG_MAIN -DNO_UI -D_WIN32 -std=gnu99 -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1366059434/vm.o ../../../source/vm.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ../../../test/sync/filagree.exe

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
