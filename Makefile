.PHONY: all delay flash clean

all: compile_commands.json delay

delay:
	bear -- ${MAKE} -C src/delay

flash:
	@test $(DIR) || (echo "DIR must be set (eg `make flash DIR=delay`)"; exit 1)
	${MAKE} -C src/$(DIR) program-dfu

clean:
	${MAKE} -C src/delay clean
