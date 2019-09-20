PROJECT = RM20

# Imported source files and paths
CONFDIR = ./config
COREDIR = .
BOARD_NAME = STM32F103x8
USE_FPU = no

include ./board/board.mk

# Compile all .c and .cpp files in the root directory
ALLCSRC += $(wildcard ./src/*.c)
ALLCPPSRC += $(wildcard ./src/*.cpp)

include $(COREDIR)/core.mk
