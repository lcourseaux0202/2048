#pragma once

#include "game.h"

#include <stdint.h>
#include <pthread.h>

#define GRID_SIZE 4
#define OBJECTIV 2048

#define CLEAR   "\e[H\e[2J\e[3J"

int proc_display(int fdDisplay);
