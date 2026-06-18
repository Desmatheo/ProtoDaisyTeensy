# Nom de ton projet (sera le nom du fichier .bin et .hex final)
TARGET = ProtoDaisyTeensy

# Fichiers sources C++
CPP_SOURCES = src/main.cpp \
              $(wildcard src/EffetEarth/Earth.cpp) \
              $(wildcard src/EffetEarth/Dattorro/*.cpp) \
              $(wildcard src/EffetEarth/Dattorro/dsp/delays/*.cpp) \
              $(wildcard src/EffetEarth/Dattorro/dsp/filters/*.cpp) \
              $(wildcard src/EffetDelay/*.cpp) \
              $(wildcard src/EffetDisto/*.cpp) \
              $(wildcard src/include/*.cpp)


# Emplacements des bibliothèques Daisy
LIBDAISY_DIR = lib/libDaisy
DAISYSP_DIR = lib/DaisySP

# Activer les modules LGPL de DaisySP (Requis pour le GranularPlayer de l'effet Uranus)
USE_DAISYSP_LGPL = 1

# Activer FatFS pour le support de la carte SD (requis pour WavHexaPlayer)
USE_FATFS = 1

# Ajout des chemins d'inclusion nécessaires (ex: pour Q, gcem, etc.)
C_INCLUDES += -Ilib/Q/q_lib/include \
              -Ilib/Q/infra/include \
              -Ilib/gcem/include

# On force le standard C++20 (nécessaire pour std::span)
CPP_STANDARD = -std=gnu++20

# On demande au compilateur de placer le programme dans les 8 Mo de la QSPI Flash
APP_TYPE = BOOT_QSPI

OPT = -O3 -ffast-math


# Emplacement du Makefile central de libDaisy qui gère toute la magie de compilation
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
