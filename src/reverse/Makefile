TARGET = reverse
CPP_SOURCES = reverse.cpp value.hpp
LIBDAISY_DIR = ../../vendor/libDaisy/
DAISYSP_DIR = ../../vendor/DaisySP/
LIBJAZZ_DIR = ../../lib/libjazz/

C_INCLUDES += -I$(LIBJAZZ_DIR)/include
LIBS += -llibjazz
LIBDIR += -L../../build/lib/libjazz
CPP_STANDARD = -std=gnu++20

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

program-dfu: ../../build/lib/libjazz/liblibjazz.a all

../../build/lib/libjazz/liblibjazz.a:
	cd ../.. && make cmake-build
