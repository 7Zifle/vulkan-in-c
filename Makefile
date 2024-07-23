# Compiler
CC = gcc
CFLAGS = -DDEBUG

# Linker flags
LDFLAGS = -I./include -lSDL2 -lSDL2_image -lSDL2_ttf -lvulkan

SRC_DIR := src
# Source files
SRCS := $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
# Output executable
TARGET = build/vk-guide

# Default build target
all: $(TARGET)

$(TARGET): $(SRCS)
	glslc -fshader-stage=vert assets/shaders/vertex.glsl -o build/vert.spv
	glslc -fshader-stage=frag assets/shaders/fragment.glsl -o build/frag.spv
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)
