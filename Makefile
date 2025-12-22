CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -Wno-unused-parameter # -Wno-unused-variable

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := bin/main

.PHONY: all clean dirs run

all: dirs $(BIN)

dirs:
	mkdir -p build bin

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	clear
	./$(BIN) $(ARGS)

clean:
	rm -rf build/* bin/*
