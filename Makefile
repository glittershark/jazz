.PHONY: all test flash clean cmake-build cmake-configure merge-compile-commands

# Default target - build everything via CMake
all: cmake-build merge-compile-commands

# Test target - build and run tests on host platform
test:
	mkdir -p build-test
	cd build-test && cmake -DTEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug ..
	cd build-test && make
	cd build-test && ctest --output-on-failure
	$(MAKE) merge-compile-commands

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

debug: deps
	@test $(DIR) || (echo "DIR must be set (eg \`make debug DIR=delay\`)"; exit 1)
	@[ -f src/$(DIR)/build/$(DIR).elf ] || (echo "You should probably build and flash first."; exit 1)
	pgrep st-util || setsid st-util -p 4242 --no-reset --semihosting >/dev/null 2>&1 &
	gdb -ex "file src/$(DIR)/build/$(DIR).elf" -ex "target remote localhost:4242"

# Dependencies for hardware projects (libDaisy)
deps: vendor/libDaisy/build/libdaisy.a vendor/DaisySP/build/libdaisysp.a

vendor/libDaisy/build/libdaisy.a: vendor/libDaisy/*
	make -C vendor/libDaisy

vendor/DaisySP/build/libdaisysp.a: vendor/DaisySP/*
	make -C vendor/DaisySP

# Merge compile commands from both build directories for clangd
merge-compile-commands:
	@if [ -f build/compile_commands.json ] && [ -f build-test/compile_commands.json ]; then \
		jq -s 'add' build/compile_commands.json build-test/compile_commands.json > compile_commands.json; \
		echo "Merged compile commands from build/ and build-test/"; \
	elif [ -f build/compile_commands.json ]; then \
		cp build/compile_commands.json compile_commands.json; \
		echo "Copied compile commands from build/"; \
	elif [ -f build-test/compile_commands.json ]; then \
		cp build-test/compile_commands.json compile_commands.json; \
		echo "Copied compile commands from build-test/"; \
	else \
		echo "No compile_commands.json found in build/ or build-test/"; \
	fi

# Clean everything
clean:
	rm -rf build build-test compile_commands.json
	${MAKE} -C src/delay clean || true
	${MAKE} -C src/reverse clean || true
	${MAKE} -C src/granular clean || true
	${MAKE} -C src/fridge clean || true
	${MAKE} -C src/identity clean || true
	${MAKE} -C src/rachel clean || true
