UNAME_S := $(shell uname -s)


ifeq ($(ARCH),)
ARCH := $(shell arch)
endif

ifeq ($(XBUILD),1)
CC = $(ARCH)-linux-gnu-gcc
C++ = $(ARCH)-linux-gnu-g++
LINK = $(ARCH)-linux-gnu-g++
else
ifeq ($(UNAME_S),Darwin)
CC = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang" 
C++ = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++" 
LINK = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++" 
else
CC = gcc
C++ = g++
LINK = g++
endif
endif

ifeq ($(UNAME_S),Darwin)
LIBGCC = 
SEH = 
IMPORTPATH=../../caLibrary/static_lib/Darwin
else
LIBGCC = -static-libgcc
SEH =  -fno-exceptions 
IMPORTPATH=./ffmpeghelper/Linux/$(ARCH)/lib
endif

LIBPATH:=objs/$(UNAME_S)/$(ARCH)
$(shell mkdir -p $(LIBPATH))
OUTPUTPATH=../../x64/debug

GCFLAGS= -O3 -w -fPIC  -Wno-unused-parameter $(SEH) -Wno-strict-aliasing -Wno-sign-compare -Wno-extra -fno-stack-protector -fvisibility=hidden

CPPFLAGS= -std=c++11 $(GCFLAGS) -I ./ffmpeghelper/ffmpeg-20190126-d8ebfd1-win64-dev/include
CFLAGS= $(GCFLAGS) -I ./ffmpeghelper/ffmpeg-20190126-d8ebfd1-win64-dev/include

LFLAGS = -fPIC -shared -Wl,-Bsymbolic -w -static-libstdc++ $(LIBGCC) -lpthread -ldl -lz -lm -fvisibility=hidden -Wl,-z,defs

ifeq ($(UNAME_S),Darwin)
TARGET=$(OUTPUTPATH)/ffmpeghelper.dylib
else
TARGET=$(OUTPUTPATH)/ffmpeghelper.so
endif

SOURCE= ./ffmpeghelper/dllmain.cpp \
		./ffmpeghelper/ffmpeghelper.cpp \
		./ffmpeghelper/stdafx.cpp


LIBOBJ=$(addprefix $(LIBPATH)/, $(addsuffix .o, $(basename $(SOURCE)))) 



IMPORTLIB=	$(IMPORTPATH)/libavformat.a\
	$(IMPORTPATH)/libavcodec.a\
	$(IMPORTPATH)/libavdevice.a\
	$(IMPORTPATH)/libavfilter.a\
	$(IMPORTPATH)/libavutil.a\
	$(IMPORTPATH)/libswresample.a\
	$(IMPORTPATH)/libswscale.a

	
all: $(TARGET)

target: $(TARGET)

lib: $(TARGET)

$(LIBPATH)/%.o: %.c
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@);  fi;\
	$(CC) $(CFLAGS) -o $@ -c $<

$(LIBPATH)/%.o: %.cpp
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi;\
	$(C++) $(CPPFLAGS) -o $@ -c $<

ifeq ($(UNAME_S),Darwin)
$(LIBPATH)/%.o: %.mm 
	if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi;\
	$(C++) $(CPPFLAGS) -o $@ -c $<
endif

$(TARGET): $(LIBOBJ)
	$(LINK) $(LFLAGS) -o $(TARGET) $(LIBOBJ) $(IMPORTLIB)

clean:
	rm -f $(TARGET) $(LIBOBJ)
