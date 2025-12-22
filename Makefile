CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -Wno-unused-parameter # -Wno-unused-variable
SDL2_FLAGS = $(shell sdl2-config --cflags --libs)

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := bin/main

.PHONY: all clean dirs run

all: dirs $(BIN)

dirs:
	mkdir -p build bin

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_FLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(SDL2_FLAGS)

run: all
	clear
	./$(BIN) $(ARGS)

clean:
	rm -rf build/* bin/*
