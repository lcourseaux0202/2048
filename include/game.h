#pragma once

#include <stdint.h>
#include <pthread.h>
#include <signal.h>

#define GRID_SIZE 4
#define OBJECTIV 2048

enum MOVE
{
    NONE = 0,
    UP,
    DOWN,
    RIGHT,
    LEFT,
    QUIT
};

enum GAMESTATUS
{
    PROGRESS,
    WIN,
    LOSE
};

enum VALIDITY
{
    VALID,
    INVALID
};

typedef struct game_variable
{
    pid_t gameId;
    int *grid;
    int score;
    char move;
    enum VALIDITY validity;
    enum GAMESTATUS status;
} game_variable;

typedef struct arg_moveAndScore
{
    game_variable *gm;
    pthread_t th_goal;
} arg_moveAndScore;

typedef struct arg_goal
{
    game_variable *gm;
    pthread_t th_main;
    int fdDisplay;
} arg_goal;

int proc_2048(char *path);
void *func_moveAndScore(void *arg);
void *func_goal(void *arg);
