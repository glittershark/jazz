# ARM cross-compilation toolchain file for Daisy Seed (STM32H7)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross-compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)
set(CMAKE_STRIP arm-none-eabi-strip)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# MCU specific flags (exactly from libDaisy Makefile)
set(MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

# Compiler flags (match libDaisy Makefile exactly)
set(CMAKE_C_FLAGS "${MCU_FLAGS} -Wall -Wno-attributes -Wno-strict-aliasing -Wno-maybe-uninitialized -Wno-missing-attributes -Wno-stringop-overflow -ggdb -O3 -fasm -fdata-sections -ffunction-sections -finline-functions -std=gnu11" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${MCU_FLAGS} -Wall -Wno-attributes -Wno-strict-aliasing -Wno-maybe-uninitialized -Wno-missing-attributes -Wno-stringop-overflow -Wno-register -ggdb -O3 -fdata-sections -ffunction-sections -finline-functions -fno-exceptions -fno-rtti -std=gnu++14" CACHE STRING "CXX flags")
set(CMAKE_ASM_FLAGS "${MCU_FLAGS} -ggdb -O3 -fdata-sections -ffunction-sections" CACHE STRING "ASM flags")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,--print-memory-usage" CACHE STRING "Linker flags")

# Processor definitions (exactly from libDaisy Makefile)
add_definitions(
    -DCORE_CM7
    -DSTM32H750xx
    -DSTM32H750IB
    -DARM_MATH_CM7
    -Dflash_layout
    -DHSE_VALUE=16000000
    -DUSE_HAL_DRIVER
    -DUSE_FULL_LL_DRIVER
    -DDATA_IN_D2_SRAM
)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# For libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)