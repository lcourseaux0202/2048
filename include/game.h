#include <stdint.h>

#define GRID_SIZE 4
#define OBJECTIV 2048

enum MOVE {
    UP,
    DOWN,
    RIGHT,
    LEFT,
    QUIT
};

enum GAMESTATUS {
    PROGRESS,
    WIN,
    LOSE
};

typedef struct game_variable{
    int * grid;
    int score;
    char move;
    enum GAMESTATUS status;
} game_variable;

int proc_2048(char * path);
void *func_moveAndScore (void * arg);
void *func_goal (void * arg);