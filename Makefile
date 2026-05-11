
PIDGIN_TREE_TOP ?= ../pidgin-2.10.11
PIDGIN3_TREE_TOP ?= ../pidgin-main
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev

WIN32_CC ?= $(WIN32_DEV_TOP)/mingw-4.7.2/bin/gcc
WIXL ?= wixl

PKG_CONFIG ?= pkg-config

CFLAGS	?= -O2 -g -pipe
LDFLAGS ?=

# Do some nasty OS and purple version detection
ifeq ($(OS),Windows_NT)
  #only defined on 64-bit windows
  PROGFILES32 = ${ProgramFiles(x86)}
  ifndef PROGFILES32
    PROGFILES32 = $(PROGRAMFILES)
  endif
  PLUGIN_TARGET = libhomeassistant.dll
  PLUGIN_DEST = "$(PROGFILES32)/Pidgin/plugins"
  PLUGIN_ICONS_DEST = "$(PROGFILES32)/Pidgin/pixmaps/pidgin/protocols"
else

  UNAME_S := $(shell uname -s)

  #.. There are special flags we need for OSX
  ifeq ($(UNAME_S), Darwin)
    #
    #.. /opt/local/include and subdirs are included here to ensure this compiles
    #   for folks using Macports.  I believe Homebrew uses /usr/local/include
    #   so things should "just work".  You *must* make sure your packages are
    #   all up to date or you will most likely get compilation errors.
    #
    INCLUDES = -I/opt/local/include -lz $(OS)

    CC = gcc
  else
    INCLUDES = 
    CC ?= gcc
	ifeq ($(UNAME_S), Linux)
		INCLUDES += -ldl
	endif
  endif

  ifeq ($(shell $(PKG_CONFIG) --exists purple-3 2>/dev/null && echo "true"),)
    ifeq ($(shell $(PKG_CONFIG) --exists purple 2>/dev/null && echo "true"),)
      PLUGIN_TARGET = FAILNOPURPLE
      PLUGIN_DEST =
	  PLUGIN_ICONS_DEST =
    else
      PLUGIN_TARGET = libhomeassistant.so
      PLUGIN_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple`
	  PLUGIN_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple`/pixmaps/pidgin/protocols
    endif
  else
    PLUGIN_TARGET = libhomeassistant3.so
    PLUGIN_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple-3`
	PLUGIN_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple-3`/pixmaps/pidgin/protocols
  endif
endif

WIN32_CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.14/include/json-glib-1.0 -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -fno-strict-aliasing -Wformat
WIN32_LDFLAGS = -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lws2_32 -g -ggdb -static-libgcc -lz
WIN32_PIDGIN2_CFLAGS = -I$(PIDGIN_TREE_TOP)/libpurple -I$(PIDGIN_TREE_TOP) $(WIN32_CFLAGS)
WIN32_PIDGIN3_CFLAGS = -I$(PIDGIN3_TREE_TOP)/libpurple -I$(PIDGIN3_TREE_TOP) -I$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_CFLAGS)
WIN32_PIDGIN2_LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple $(WIN32_LDFLAGS)
WIN32_PIDGIN3_LDFLAGS = -L$(PIDGIN3_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_LDFLAGS) -lgplugin

C_FILES := \
	homeassistant_connection.c \
	homeassistant_message.c \
	homeassistant_websocket.c
	
PURPLE_COMPAT_FILES := purple2compat/http.c purple2compat/purple-socket.c
PURPLE_C_FILES := libhomeassistant.c $(C_FILES)



.PHONY:	all install FAILNOPURPLE clean installer install-icons build-locales %-locale-install

all: $(PLUGIN_TARGET)

libhomeassistant.so: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(CC) -fPIC $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) `$(PKG_CONFIG) purple glib-2.0 json-glib-1.0 zlib --libs --cflags` $(INCLUDES) -Ipurple2compat -g -ggdb

libhomeassistant3.so: $(PURPLE_C_FILES)
	$(CC) -fPIC $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) `$(PKG_CONFIG) purple-3 glib-2.0 json-glib-1.0 zlib --libs --cflags` $(INCLUDES) -g -ggdb

#version.o:
#	i686-w64-mingw32-windres -D VERSION_TAG=`date +"%-Y,%-m,%-d,%-H%-M"` version.rc -O coff -o version.o

libhomeassistant.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES) #version.o
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

libhomeassistant3.dll: $(PURPLE_C_FILES) #version.o
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN3_CFLAGS) $(WIN32_PIDGIN3_LDFLAGS)

install: $(PLUGIN_TARGET) install-icons
	mkdir -p $(PLUGIN_DEST)
	install -p $(PLUGIN_TARGET) $(PLUGIN_DEST)

install-icons: homeassistant16.png homeassistant22.png homeassistant48.png
	mkdir -p $(PLUGIN_ICONS_DEST)/16
	mkdir -p $(PLUGIN_ICONS_DEST)/22
	mkdir -p $(PLUGIN_ICONS_DEST)/48
	install homeassistant16.png $(PLUGIN_ICONS_DEST)/16/homeassistant.png
	install homeassistant22.png $(PLUGIN_ICONS_DEST)/22/homeassistant.png
	install homeassistant48.png $(PLUGIN_ICONS_DEST)/48/homeassistant.png

FAILNOPURPLE:
	echo "You need libpurple development headers installed to be able to compile this plugin"

clean:
	rm -f $(PLUGIN_TARGET)

po/purple-homeassistant.pot: $(PURPLE_C_FILES)
	xgettext $^ -k_ --no-location -o $@

po/%.po: po/purple-homeassistant.pot
	msgmerge $@ po/purple-homeassistant.pot > tmp-$*
	mv -f tmp-$* $@

po/%.mo: po/%.po
	msgfmt -o $@ $^

build-locales: $(LOCALES)

%-locale-install: po/%.mo
	mkdir -m $(DIR_PERM) -p $(LOCALE_DEST)/$(*F)/LC_MESSAGES
	install -m $(FILE_PERM) -p po/$(*F).mo $(LOCALE_DEST)/$(*F)/LC_MESSAGES/purple-homeassistant.mo


installer: purple-homeassistant.wxs libhomeassistant.dll
	$(WIXL) -I . -v purple-homeassistant.wxs -o purple-homeassistant.msi --arch x86
