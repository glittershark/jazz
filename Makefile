.PHONY: all deps delay flash clean

all: compile_commands.json delay

delay: deps
	bear -- ${MAKE} -C src/delay

flash:
	@test $(DIR) || (echo "DIR must be set (eg `make flash DIR=delay`)"; exit 1)
	${MAKE} -C src/$(DIR) program-dfu

deps: vendor/libDaisy/build/libdaisy.a vendor/DaisySP/build/libdaisysp.a

vendor/libDaisy/build/libdaisy.a: vendor/libDaisy/*
	make -C vendor/libDaisy

vendor/DaisySP/build/libdaisysp.a: vendor/DaisySP/*
	make -C vendor/libDaisy

clean:
	${MAKE} -C src/delay clean
