# Developer's makefile for building Lua
# see luaconf.h for further customization

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Warnings valid for both C and C++
CWARNSCPP= \
	-Wfatal-errors \
	-Wextra \
	-Wshadow \
	-Wundef \
	-Wwrite-strings \
	-Wredundant-decls \
	-Wdisabled-optimization \
	-Wdouble-promotion \
	-Wmissing-declarations \
        # the next warnings might be useful sometimes,
	# but usually they generate too much noise
	# -Werror \
	# -pedantic   # warns if we use jump tables \
	# -Wconversion  \
	# -Wsign-conversion \
	# -Wstrict-overflow=2 \
	# -Wformat=2 \
	# -Wcast-qual \


# Warnings for gcc, not valid for clang
CWARNGCC= \
	-Wlogical-op \
	-Wno-aggressive-loop-optimizations \
	-Wno-inline \
	-Wno-ignored-qualifiers \


# The next warnings are neither valid nor needed for C++
CWARNSC= -Wdeclaration-after-statement \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wstrict-prototypes \
	-Wc++-compat \
	-Wold-style-definition \

# Some useful compiler options for internal tests:
# -DLUAI_ASSERT turns on all assertions inside Lua.
# -DHARDSTACKTESTS forces a reallocation of the stack at every point where
# the stack can be reallocated.
# -DHARDMEMTESTS forces a full collection at all points where the collector
# can run.
# -DEMERGENCYGCTESTS forces an emergency collection at every single allocation.
# -DEXTERNMEMCHECK removes internal consistency checking of blocks being
# deallocated (useful when an external tool like valgrind does the check).
# -DMAXINDEXRK=k limits range of constants in RK instruction operands.
# -DLUA_COMPAT_5_3

# -pg -malign-double
# -DLUA_USE_CTYPE -DLUA_USE_APICHECK

# The following options help detect "undefined behavior"s that seldom
# create problems; some are only available in newer gcc versions. To
# use some of them, we also have to define an environment variable
# ASAN_OPTIONS="detect_invalid_pointer_pairs=2".
# -fsanitize=undefined
# -fsanitize=pointer-subtract -fsanitize=address -fsanitize=pointer-compare
# TESTS= -DLUA_USER_H='"ltests.h"' -O0 -g

# Your platform. See PLATS for possible values.
PLAT= guess

# See LUA_C_LINKAGE definition in source. If "CC" replaces gcc with g++, then
# LUA_LINKAGE needs to be undefined
# LUA_LINKAGE= -DLUA_C_LINKAGE

CC= g++ -std=c++11 $(CWARNSCPP) $(CWARNGCC)
CPP= g++ -std=c++11 $(CWARNSCPP) $(CWARNGCC)
CFLAGS= -O2 -Wall -Wextra -DNDEBUG -DLUA_COMPAT_5_3 $(SYSCFLAGS) $(MYCFLAGS)
CPERF_FLAGS = -O3 -march=native -fno-plt -fno-stack-protector -ffast-math -fno-finite-math-only # -flto
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

AR= ar rc
RANLIB= ranlib
RM= rm -f
UNAME= uname

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

LUA_PATCHES = -DLUA_C99_MATHLIB  \
		-DLUA_USE_LONGJMP \
		-DGRIT_COMPAT_IPAIRS \
		-DGRIT_POWER_DEFER_OLD \
		-DGRIT_POWER_COMPOUND \
		-DGRIT_POWER_INTABLE \
		-DGRIT_POWER_TABINIT \
		-DGRIT_POWER_SAFENAV \
		-DGRIT_POWER_CCOMMENT \
		-DGRIT_POWER_JOAAT \
		-DGRIT_POWER_EACH \
		-DGRIT_POWER_WOW \
		-DGRIT_POWER_CHRONO \
		-DGRIT_POWER_BLOB \
		-DGRIT_POWER_READLINE_HISTORY \

GLM_FLAGS = -DGLM_ENABLE_EXPERIMENTAL \
		-DGLM_FORCE_INTRINSICS \
		-DGLM_FORCE_INLINE \
		-DGLM_FORCE_Z_UP \
		-DLUA_GLM_INCLUDE_ALL \
		-DLUA_GLM_ALIASES \
		-DLUA_GLM_GEOM_EXTENSIONS \
		-DLUA_GLM_RECYCLE \
		# -DGLM_FORCE_QUAT_DATA_XYZW \

MYCFLAGS= $(TESTS) $(LUA_PATCHES) $(GLM_FLAGS) -Ilibs/glm/
MYLDFLAGS= $(TESTS)
MYLIBS=
MYOBJS=

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= guess aix bsd freebsd generic linux linux-readline macos mingw posix solaris

LUA_A=	liblua.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o ltests.o lglm.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

LUA_T=	lua
LUA_O=	lua.o

LUAC_T=	luac
LUAC_O=	luac.o

ALL_O= $(BASE_O) $(LUA_O) $(LUAC_O)
ALL_T= $(LUA_A) $(LUA_T) $(LUAC_T)
ALL_A= $(LUA_A)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(LUA_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

$(LUAC_T): $(LUAC_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(LUAC_O) $(LUA_A) $(LIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O) onelua.o $(GLM_A)

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CPP= $(CPP)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(SYSLDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"
	@echo "UNAME= $(UNAME)"

$(ALL_O): makefile ltests.h

# Convenience targets for popular platforms.
ALL= all

help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See doc/readme.html for complete instructions."

guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

AIX aix:
	$(MAKE) $(ALL) CC="xlc" CFLAGS="-O2 -DLUA_USE_POSIX -DLUA_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN" SYSLIBS="-Wl,-E"

FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX -DLUA_USE_READLINE -I/usr/include/edit" SYSLIBS="-Wl,-E -ledit" CC="cc"

generic: $(ALL)

Linux linux:	linux-noreadline

linux-noreadline:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX" SYSLIBS="-Wl,-E -ldl"

linux-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX -DLUA_USE_READLINE" SYSLIBS="-Wl,-E -ldl -lreadline"

linux-one:
	$(MAKE) linux-readline CC="$(CPP)" LUA_O="onelua.o" BASE_O="onelua.o" CORE_O="" LIB_O="" LUAC_T="" MYCFLAGS="$(MYCFLAGS) $(CPERF_FLAGS) -I. -Ilibs/glm-binding"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_MACOSX"

macos-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_MACOSX -DLUA_USE_READLINE" SYSLIBS="-lreadline"

mingw:
	$(MAKE) "LUA_A=lua54.dll" "LUA_T=lua.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-Wno-attributes -DLUA_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" lua.exe
	$(MAKE) "LUAC_T=luac.exe" luac.exe

posix:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX"

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help test clean default o a depend echo

lglm.o: lglm.cpp lua.h luaconf.h lglm.hpp lua.hpp lualib.h \
 lauxlib.h lglm_core.h llimits.h ltm.h lobject.h lglm_string.hpp \
 lgrit_lib.h lapi.h lstate.h lzio.h lmem.h ldebug.h lfunc.h lgc.h \
 lstring.h ltable.h lvm.h ldo.h
	$(CPP) $(LUA_LINKAGE) $(CFLAGS) $(CPERF_FLAGS) $(TESTS) -c -o lglm.o lglm.cpp

# lua-glm binding
GLM_A = glm.so

lib-glm:
	$(CPP) $(LUA_LINKAGE) $(CFLAGS) $(CPERF_FLAGS) $(TESTS) -fPIC -I. -Ilibs/glm-binding -shared -o $(GLM_A) libs/glm-binding/lglmlib.cpp $(LIBS)

lib-glm-mingw:
	$(MAKE) lib-glm SYSCFLAGS="-L . -DLUA_BUILD_AS_DLL" GLM_A="glm.dll" SYSLIBS="-llua" SYSLDFLAGS="-s"

lib-glm-macos:
	$(MAKE) lib-glm SYSCFLAGS="-L . -DLUA_USE_MACOSX" SYSLIBS="-llua"

# GLM binding library built as an object; disabled by default.
# -I. -Ilibs/glm/ -Ilibs/glm-binding
# -DLUA_INCLUDE_LIBGLM
# lglmlib.o
lglmlib.o: libs/glm-binding/lglmlib.cpp lglm.hpp lua.hpp lua.h luaconf.h \
 lualib.h lauxlib.h libs/glm-binding/api.hpp \
 libs/glm-binding/bindings.hpp lapi.h llimits.h lstate.h lobject.h \
 lglm.hpp ltm.h lzio.h lmem.h lgc.h lvm.h ldo.h lobject.h lstate.h \
 lglm_core.h libs/glm-binding/ext/vector_extensions.hpp \
 libs/glm-binding/ext/matrix_extensions.hpp \
 libs/glm-binding/ext/quat_extensions.hpp \
 libs/glm-binding/ext/vector_extensions.hpp \
 libs/glm-binding/ext/matrix_extensions.hpp lua.hpp \
 libs/glm-binding/lglmlib.hpp lua.h libs/glm-binding/lglmlib_reg.hpp
	$(CC) $(CFLAGS) $(CPERF_FLAGS) -I. -Ilibs/glm-binding -c -o lglmlib.o libs/glm-binding/lglmlib.cpp

# DO NOT EDIT
# automatically made with 'g++ -MM l*.c'

lapi.o: lapi.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldebug.h \
 ldo.h lfunc.h lgc.h lglm_core.h lstring.h ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lprefix.h lua.h luaconf.h lauxlib.h lglm.hpp \
 lualib.h
lbaselib.o: lbaselib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 lglm.hpp lglm_core.h llimits.h ltm.h lobject.h
lcode.o: lcode.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h lzio.h lmem.h lopcodes.h \
 lparser.h ldebug.h lstate.h ltm.h ldo.h lgc.h lstring.h ltable.h lvm.h
lcorolib.o: lcorolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lctype.o: lctype.c lprefix.h lctype.h lua.h luaconf.h llimits.h
ldblib.o: ldblib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
ldebug.o: ldebug.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h \
 lcode.h llex.h lopcodes.h lparser.h ldebug.h ldo.h lfunc.h lstring.h \
 lgc.h ltable.h lvm.h
ldo.o: ldo.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h \
 ldebug.h ldo.h lfunc.h lgc.h lglm_core.h lopcodes.h lparser.h \
 lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.c lprefix.h lua.h luaconf.h lobject.h llimits.h lglm.hpp \
 lualib.h lauxlib.h lstate.h ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h \
 lfunc.h lgc.h
lgc.o: lgc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h \
 lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lprefix.h lua.h luaconf.h lualib.h lauxlib.h
liolib.o: liolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
llex.o: llex.c lprefix.h lua.h luaconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h \
 lmem.h ldo.h lgc.h llex.h lparser.h lstring.h ltable.h
lmathlib.o: lmathlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lmem.o: lmem.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h \
 lgc.h
loadlib.o: loadlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lobject.o: lobject.c lprefix.h lua.h luaconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h lglm.hpp lualib.h lauxlib.h ltm.h \
 lzio.h lmem.h ldo.h lglm_core.h lstring.h lgc.h lvm.h
lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h lua.h luaconf.h
loslib.o: loslib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lparser.o: lparser.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h lzio.h lmem.h lopcodes.h \
 lparser.h ldebug.h lstate.h ltm.h ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h \
 ldebug.h ldo.h lfunc.h lgc.h llex.h lstring.h ltable.h
lstring.o: lstring.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h \
 lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
ltable.o: ltable.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h lgc.h \
 lstring.h ltable.h lvm.h
ltablib.o: ltablib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 lglm_core.h llimits.h ltm.h lobject.h lglm.hpp lua.hpp
ltests.o: ltests.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h \
 lcode.h llex.h lopcodes.h lparser.h lctype.h ldebug.h ldo.h lfunc.h \
 lopnames.h lstring.h lgc.h ltable.h
ltm.o: ltm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h \
 lgc.h lglm_core.h lstring.h ltable.h lvm.h
lua.o: lua.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
luac.o: luac.c lprefix.h lua.h luaconf.h lauxlib.h ldebug.h lstate.h \
 lobject.h llimits.h lglm.hpp lualib.h ltm.h lzio.h lmem.h \
 lopcodes.h lopnames.h lundump.h
lundump.o: lundump.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h \
 lmem.h ldo.h lfunc.h lstring.h lgc.h lundump.h
lutf8lib.o: lutf8lib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lvm.o: lvm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h lmem.h ldo.h \
 lfunc.h lgc.h lglm_core.h lopcodes.h lstring.h ltable.h lvm.h \
 ljumptab.h
lzio.o: lzio.c lprefix.h lua.h luaconf.h llimits.h lmem.h lstate.h \
 lobject.h lglm.hpp lualib.h lauxlib.h ltm.h lzio.h
onelua.o: onelua.c lprefix.h luaconf.h lzio.c lua.h llimits.h lmem.h \
 lstate.h lobject.h lglm.hpp lua.hpp lualib.h lauxlib.h ltm.h lzio.h \
 lctype.c lctype.h lopcodes.c lopcodes.h lmem.c ldebug.h ldo.h lgc.h \
 lundump.c lfunc.h lstring.h lundump.h ldump.c lstate.c lapi.h llex.h \
 ltable.h lgc.c llex.c lparser.h lcode.c lcode.h lvm.h lparser.c ldebug.c \
 lfunc.c lglm.cpp lglm_core.h lgrit_lib.h lobject.c ltm.c lstring.c \
 ltable.c ldo.c lvm.c ljumptab.h lapi.c lauxlib.c lbaselib.c lcorolib.c \
 ldblib.c liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c \
 lutf8lib.c linit.c lua.c

# (end of Makefile)
