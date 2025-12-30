#include <stdint.h>

#define GRID_SIZE 4

enum MOVE {
    UP,
    DOWN,
    RIGHT,
    LEFT,
    QUIT
};

int proc_2048(char * path);
void *func_moveAndScore (void * arg);
void *func_goal (void * arg);