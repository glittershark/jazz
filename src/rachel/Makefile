TARGET = rachel
CPP_SOURCES = rachel.cpp
LIBDAISY_DIR = ../../vendor/libDaisy/
DAISYSP_DIR = ../../vendor/DaisySP/

C_INCLUDES += -I$(LIBJAZZ_DIR)/include
LIBS += -llibjazz
LIBDIR += -L../../build/lib/libjazz
CPP_STANDARD = -std=gnu++20

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

program-dfu: ../../build/lib/libjazz/liblibjazz.a all

../../build/lib/libjazz/liblibjazz.a:
	cd ../.. && make cmake-build
