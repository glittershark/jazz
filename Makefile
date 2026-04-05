.PHONY: all test console flash st-flash logs debug clean compile-commands fmt fmt-check

SOURCES := $(shell find src lib tests -name '*.cpp' -o -name '*.hpp' -o -name '*.h' | grep -v -e vendor -e CMakeFiles)

# Build for ARM
all:
	cmake --preset arm
	cmake --build --preset arm
	$(MAKE) compile-commands

# Build and run tests
test:
	cmake --preset host
	cmake --build --preset host
	ctest --test-dir build-test --output-on-failure
	$(MAKE) compile-commands

# Build the host-side audio CLI
console:
	cmake --preset host
	cmake --build --preset host --target jazz-console
	$(MAKE) compile-commands

# Flash via ST-Link
flash:
	@test -n "$(DIR)" || (echo "Usage: make flash DIR=fridge"; exit 1)
	st-flash write build/src/$(DIR)/$(DIR).bin 0x08000000
	st-flash reset

# Alias for flash
st-flash: flash

# Serial logs
logs:
	st-flash reset
	@echo "Waiting for USB..."
	@while ! ls /dev/ttyACM[0-9] >/dev/null 2>&1; do sleep 0.1; done
	@sleep 1
	cat /dev/ttyACM[0-9]

# GDB debugging
debug:
	@test -n "$(DIR)" || (echo "Usage: make debug DIR=fridge"; exit 1)
	pgrep st-util || setsid st-util -p 4242 --no-reset --semihosting >/dev/null 2>&1 &
	gdb -ex "file build/src/$(DIR)/$(DIR).elf" -ex "target remote localhost:4242"

# Merge compile_commands.json from both builds
compile-commands:
	@if [ -f build/compile_commands.json ] && [ -f build-test/compile_commands.json ]; then \
		jq -s 'add | unique_by(.file)' build/compile_commands.json build-test/compile_commands.json > compile_commands.json; \
		echo "Merged compile_commands.json"; \
	elif [ -f build/compile_commands.json ]; then \
		cp build/compile_commands.json .; \
	elif [ -f build-test/compile_commands.json ]; then \
		cp build-test/compile_commands.json .; \
	fi


# Format code in place
fmt:
	clang-format -i $(SOURCES)

# Check formatting (for CI)
fmt-check:
	clang-format --dry-run --Werror $(SOURCES)

clean:
	rm -rf build build-test compile_commands.json
