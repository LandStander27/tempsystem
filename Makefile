.PHONY: all build clean

all:
	make build

build:
	mkdir -p build
	cd build && cmake ..
	make -C build

run:
	./build/tempsystem

clean:
	rm -rf build