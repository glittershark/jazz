# Jazz Project - Development Notes

## Building

Build a firmware project:
```bash
make -C src/fridge          # Just build
make flash DIR=fridge       # Build and flash via DFU (requires boot mode)
make st-flash DIR=fridge    # Build and flash via STLink
```

## Flashing

Two methods available:

1. **DFU** (requires holding BOOT button during reset):
   ```bash
   make flash DIR=fridge
   ```

2. **STLink** (always works when connected):
   ```bash
   make st-flash DIR=fridge
   ```
   Or manually:
   ```bash
   st-flash write src/fridge/build/fridge.bin 0x08000000
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
arm-none-eabi-gdb -ex "file src/fridge/build/fridge.elf" -ex "target remote localhost:4242"
```

Useful GDB commands:
- `bt` - backtrace
- `monitor reset halt` - reset and halt the CPU
- `continue` - resume execution
- `break HardFault_Handler` - break on crashes
