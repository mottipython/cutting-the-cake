# Cutting the Cake - a screensaver of Calca 1 from Neal Stephenson's "Anathem"
VERSION  := 0.1.0
PREFIX   ?= /usr
SYSCONFDIR ?= /etc
DESTDIR  ?=
CC       ?= cc
CFLAGS   ?= -O2 -g
override CFLAGS += -std=gnu11 -Wall -Isrc
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

all: build/cutting-the-cake

# the pictures in assets/art are built into the program, so it must be compiled from here
build/cutting-the-cake: src/cake.c src/stb_image.h $(wildcard assets/art/*)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -o $@ src/cake.c $(LDFLAGS) $(SDL_LIBS) -lm

check: build/cutting-the-cake
	SDL_VIDEODRIVER=dummy ./build/cutting-the-cake --snapshot build/check.bmp --time 90 --size 640x400 && echo "ran a full round"

clean:
	rm -rf build dist

.PHONY: all check clean
