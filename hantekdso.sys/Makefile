

WINEPREFIX		= .wine_hantek
USER					= alexandr
WINE_LIBS_PATH = /opt/wine-stable/lib/wine/




SRCDIR                = .
SUBDIRS               =
DLLS                  = hantekdso.sys
LIBS                  =
EXES                  =



### Common settings

CEXTRA                = -m32
CXXEXTRA              = -m32
RCEXTRA               =
DEFINES               =
INCLUDE_PATH          =
DLL_PATH              =
DLL_IMPORTS           = ntoskrnl
LIBRARY_PATH          =
LIBRARIES             = -lusb-1.0 \
			-lpthread


### hantekdso.sys sources and settings

hantekdso_dll_MODULE  = hantekdso.sys
hantekdso_dll_C_SRCS  = src/errors.c \
			src/hantekdso.c \
			src/libusbdso.c
hantekdso_dll_CXX_SRCS=
hantekdso_dll_RC_SRCS =
hantekdso_dll_LDFLAGS = -shared \
			$(hantekdso_dll_MODULE:.sys=.spec) \
			-m32 \
			-Wb,--subsystem,native
hantekdso_dll_ARFLAGS =
hantekdso_dll_DLL_PATH=
hantekdso_dll_DLLS    = 
hantekdso_dll_LIBRARY_PATH=
hantekdso_dll_LIBRARIES=

hantekdso_dll_OBJS    = $(hantekdso_dll_C_SRCS:.c=.o) \
			$(hantekdso_dll_CXX_SRCS:.cpp=.o) \
			$(hantekdso_dll_RC_SRCS:.rc=.res)



### Global source lists

C_SRCS                = $(hantekdso_dll_C_SRCS)
CXX_SRCS              = $(hantekdso_dll_CXX_SRCS)
RC_SRCS               = $(hantekdso_dll_RC_SRCS)


### Tools

CC = winegcc
CXX = wineg++
RC = wrc
AR = ar


### Generic targets

all: $(SUBDIRS) $(DLLS:%=%.so) $(DLLS:%=%.fake) $(LIBS) $(EXES)

### Build rules

.PHONY: all clean dummy

$(SUBDIRS): dummy
	@cd $@ && $(MAKE)

# Implicit rules

.SUFFIXES: .cpp .cxx .rc .res
DEFINCL = $(INCLUDE_PATH) $(DEFINES) $(OPTIONS)

.c.o:
	$(CC) -c $(CFLAGS) $(CEXTRA) $(DEFINCL) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CXXEXTRA) $(DEFINCL) -o $@ $<

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(CXXEXTRA) $(DEFINCL) -o $@ $<

.rc.res:
	$(RC) $(RCFLAGS) $(RCEXTRA) $(DEFINCL) -fo$@ $<

# Rules for cleaning

CLEAN_FILES     = y.tab.c y.tab.h lex.yy.c core *.orig *.rej \
                  \\\#*\\\# *~ *% .\\\#*

clean:: $(SUBDIRS:%=%/__clean__) $(EXTRASUBDIRS:%=%/__clean__)
	$(RM) $(CLEAN_FILES) $(RC_SRCS:.rc=.res) $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)
	$(RM) $(DLLS:%=%.so) $(DLLS:%=%.fake) $(LIBS) $(EXES) $(EXES:%=%.so) $(EXES:%=%.fake)

$(SUBDIRS:%=%/__clean__): dummy
	cd `dirname $@` && $(MAKE) clean

$(EXTRASUBDIRS:%=%/__clean__): dummy
	-cd `dirname $@` && $(RM) $(CLEAN_FILES)

### Target specific build rules
DEFLIB = $(LIBRARY_PATH) $(LIBRARIES) $(DLL_PATH) $(DLL_IMPORTS:%=-l%)

$(hantekdso_dll_MODULE).so $(hantekdso_dll_MODULE).fake: $(hantekdso_dll_OBJS) $(hantekdso_dll_MODULE:.sys=.spec)
	$(CC) $(hantekdso_dll_LDFLAGS) -o $@ $(hantekdso_dll_OBJS) $(hantekdso_dll_LIBRARY_PATH) $(hantekdso_dll_DLL_PATH) $(DEFLIB) $(hantekdso_dll_DLLS:%=-l%) $(hantekdso_dll_LIBRARIES:%=-l%)


install:
	cp $(hantekdso_dll_MODULE).fake /home/$(USER)/$(WINEPREFIX)/drive_c/windows/syswow64/drivers/$(hantekdso_dll_MODULE)
	chown $(USER):$(USER) /home/$(USER)/$(WINEPREFIX)/drive_c/windows/syswow64/drivers/$(hantekdso_dll_MODULE)
	cp $(hantekdso_dll_MODULE).so $(WINE_LIBS_PATH)
#	sudo -u $(USER) sh -c "WINEPREFIX=/home/$(USER)/$(WINEPREFIX) wine regedit.exe ./resources/hantekdso.reg"
