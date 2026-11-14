# RGS MUSIC — PS4 / OpenOrbis
TITLE       := RGS MUSIC
VERSION     := 0.03
TITLE_ID    := RIDA00001
CONTENT_ID  := IV0000-RIDA00001_00-RIDAMUSIC0000000

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
CREATE_FSELF ?= $(TOOLCHAIN)/bin/linux/create-fself
CREATE_GP4   ?= $(TOOLCHAIN)/bin/linux/create-gp4
PKGTOOL      := $(TOOLCHAIN)/bin/linux/PkgTool.Core

PROJDIR     := src
INTDIR      := $(PROJDIR)/x64/Debug

# Même options et bibliothèques système que BLACKOUT, qui démarre correctement.
LIBS        := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceSysmodule
EXTRAFLAGS  := -O2 -Wall
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.c,$(INTDIR)/%.o,$(CFILES)) \
               $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

ifeq ($(shell uname -s),Linux)
CC := clang
CXX := clang++
LD := ld.lld
else
$(error This automated build currently targets Linux)
endif

RUNTIME_PRX := $(sort $(wildcard sce_module/*.prx))

.PHONY: all clean
all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(PROJDIR)/%.cpp | $(INTDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/rida_music.elf $(LDFLAGS)
	test -x $(CREATE_FSELF)
	$(CREATE_FSELF) -in=$(INTDIR)/rida_music.elf -out=$(INTDIR)/rida_music.oelf -eboot=$@ -paid=0x3800000000000011
	test -s $@
	od -An -N4 -tx1 "$@" | grep -q "4f 15 3d 1d"

sce_sys/param.sfo: Makefile
	mkdir -p sce_sys
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(wildcard assets/*) $(RUNTIME_PRX)
	$(CREATE_GP4) -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOL) pkg_build $< .

clean:
	rm -rf $(CONTENT_ID).pkg pkg.gp4 eboot.bin sce_sys/param.sfo $(INTDIR)
