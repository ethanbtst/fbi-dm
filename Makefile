# .DEFAULT_GOAL := all

CC = gcc
PKG_CONFIG ?= pkg-config
PKGS = pam openlibm

CDIR = src
IDIR = include
BDIR = build

CPPFLAGS := -I$(IDIR) ${shell $(PKG_CONFIG) --cflags $(PKGS)}
CFLAGS := -Wall -Wextra 
DEBUG_FLAGS := -Wall -Wextra -g
LDLIBS := $(shell $(PKG_CONFIG) --libs $(PKGS))

TARGET = fb-dm
DEPS = $(wildcard $(IDIR)/*.h)
SRCS = $(wildcard $(CDIR)/*.c)
OBJS = $(SRCS:$(CDIR)/%.c=$(BDIR)/%.o)

all: $(TARGET)

.PHONY: all debug clean install uninstall

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BDIR)/%.o: $(CDIR)/%.c $(DEPS)
	mkdir -p $(BDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(DEBUG_FLAGS)" $(TARGET)

clean:
	rm -rf $(BDIR)
	rm -f $(CDIR)/*.o
	rm -f $(TARGET)

uninstall:
	rm -rf /opt/fb-dm

install:
	mkdir -p /opt/fb-dm
	cp -f fb-dm /opt/fb-dm
	cp -rf img /opt/fb-dm
	cp config.ini /opt/fb-dm
	cp -f fb-dm@.service /usr/lib/systemd/system
