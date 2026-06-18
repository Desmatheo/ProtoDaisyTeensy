# Project Name
TARGET = DelayEffet

# Sources
# Use 'wildcard' to automatically find all .cpp files.
CPP_SOURCES = Delay.cpp

# Library Locations
LIBDAISY_DIR = ../../lib/libDaisy
DAISYSP_DIR = ../../lib/DaisySP

# The Q library and std::span require modern C++
CPP_STANDARD = -std=gnu++20

# Add the current directory and Q library paths to the compiler's search path
C_INCLUDES += -I. -I./../../lib/Q/q_lib/include -I./../../lib/Q/infra/include -I./../../lib/gcem/include -I./CloudSeed 

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
