# Project Name
TARGET = testEarthHexa

# Sources
# Use 'wildcard' to automatically find all .cpp files.
CPP_SOURCES = Earth.cpp
CPP_SOURCES += $(wildcard Dattorro/*.cpp)
CPP_SOURCES += $(wildcard Dattorro/dsp/delays/*.cpp)
CPP_SOURCES += $(wildcard Dattorro/dsp/filters/*.cpp)

# Library Locations
LIBDAISY_DIR = ../../lib/libDaisy
DAISYSP_DIR = ../../lib/DaisySP

# The Q library and std::span require modern C++
CPP_STANDARD = -std=gnu++20

# Add the current directory and Q library paths to the compiler's search path
C_INCLUDES += -I. -I../../lib/Q/q_lib/include -I../../lib/Q/infra/include -I../../lib/gcem/include 

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
