# Jazz Project - Development Notes

## Building

Build all ARM firmware:
```bash
make                        # Build all firmware for ARM
```

Build and run tests:
```bash
make test                   # Build for host and run tests
```

## Flashing

Flash via STLink (always works when connected):
```bash
make flash DIR=fridge
```
Or manually:
```bash
st-flash write build/src/fridge/fridge.bin 0x08000000
st-flash reset
```

## Serial Logs

The Daisy outputs logs via USB serial. Device appears at `/dev/ttyACM*` (number varies after reset).

```bash
make logs                   # Reset board and follow serial output (Ctrl+C to stop)
```

Or manually:
```bash
st-flash reset
sleep 2
cat /dev/ttyACM[0-9]
```

Note: With `StartLog(true)`, the board waits for a serial connection before continuing.

## Debugging with GDB

```bash
make debug DIR=fridge
```

This starts `st-util` (GDB server) and connects GDB. The board must be flashed first.

Manual GDB commands:
```bash
st-util -p 4242 --semihosting &
arm-none-eabi-gdb -ex "file build/src/fridge/fridge.elf" -ex "target remote localhost:4242"
```

Useful GDB commands:
- `bt` - backtrace
- `monitor reset halt` - reset and halt the CPU
- `continue` - resume execution
- `break HardFault_Handler` - break on crashes
