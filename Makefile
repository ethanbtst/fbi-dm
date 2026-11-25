# .DEFAULT_GOAL := all

CC = gcc

CDIR = src
IDIR = include
BDIR = build

CFLAGS := -Wall -Wextra -I$(IDIR) -pthread -O3 
DEBUG_FLAGS := -Wall -Wextra -g -pthread
LDLIBS := -lm -lpam

TARGET = fbi-dm
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
	rm -f /usr/bin/fbi-dm
	rm -rf /etc/fbi-dm
	rm -f /usr/lib/systemd/system/fbi-dm@.service

install:
	install -Dm755 fbi-dm /usr/bin/fbi-dm
	install -Dm644 res/config.ini /etc/fbi-dm/config.ini
	install -Dm644 res/fbi-dm@.service /usr/lib/systemd/system/fbi-dm@.service
	install -dm755 /etc/fbi-dm/img
	cp -rf img/. /etc/fbi-dm/img/