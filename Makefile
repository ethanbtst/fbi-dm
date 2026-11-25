# .DEFAULT_GOAL := all

CC = gcc

CDIR = src
IDIR = include
BDIR = build

CFLAGS := -Wall -Wextra -I$(IDIR) -pthread -O3 -march=native -flto -mtune=native -ffast-math
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
	rm -rf /opt/fbi-dm

install:
	mkdir -p /opt/fbi-dm
	cp -f fbi-dm /opt/fbi-dm
	cp -rf img /opt/fbi-dm
	cp res/config.ini /opt/fbi-dm
	cp -f res/fbi-dm@.service /usr/lib/systemd/system