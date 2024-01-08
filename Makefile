.PHONY: build install uninstall clean all

prefix ?= /usr/local

build:
	cmake --preset "default"
	cmake --build build --preset "default"

install:
	install -D -m 644 ./build/bittorrent $(DESTDIR)$(prefix)/usr/bin/bittorrent

uninstall:
	rm -rf $(DESTDIR)$(prefix)/usr/bin/bittorrent

clean:
	rm -r build

all: build
