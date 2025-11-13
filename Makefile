.PHONY: all test flash clean cmake-build cmake-configure

# Default target - build everything via CMake
all: cmake-build

# Test target - build and run tests on host platform
test:
	rm -rf build-test
	mkdir -p build-test
	cd build-test && cmake -DTEST_ONLY=ON ..
	cd build-test && make
	cd build-test && ctest

# Create build directory and configure CMake
cmake-configure: build/Makefile

build/Makefile:
	mkdir -p build
	cd build && cmake ..

# Build using CMake
cmake-build: cmake-configure
	cd build && make

# Flash target - build hardware project and flash to device
flash: deps
	@test $(DIR) || (echo "DIR must be set (eg \`make flash DIR=delay\`)"; exit 1)
	${MAKE} -C src/$(DIR) program-dfu

# Dependencies for hardware projects (libDaisy)
deps: vendor/libDaisy/build/libdaisy.a vendor/DaisySP/build/libdaisysp.a

vendor/libDaisy/build/libdaisy.a: vendor/libDaisy/*
	make -C vendor/libDaisy

vendor/DaisySP/build/libdaisysp.a: vendor/DaisySP/*
	make -C vendor/libDaisy

# Clean everything
clean:
	rm -rf build build-test
	${MAKE} -C src/delay clean || true
	${MAKE} -C src/reverse clean || true
	${MAKE} -C src/granular clean || true
