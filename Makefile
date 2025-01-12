.PHONY: all build clean
version := $(shell ( set -o pipefail; git describe --long --abbrev=7 2>/dev/null | sed 's/\([^-]*-g\)/r\1/;s/-/./g' || printf "r%s.%s" "$$(git rev-list --count HEAD)" "$$(git rev-parse --short=7 HEAD)"))

all:
	make build

build:
	sed -i "s/version = .*;/version = \"$(version)\";/" include/info.hpp
	
	mkdir -p build
	cd build && cmake ..
	make -C build

run:
	./build/tempsystem

clean:
	rm -rf build