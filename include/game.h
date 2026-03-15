#pragma once
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#define MAX_GAME_NB 64
#define GRID_SIZE   4
#define OBJECTIV    2048

#define SEGMENT_PATH "/bin"
#define PROJECT_ID 29 

enum MOVE
{
    NONE = 0,
    START,
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

typedef struct message
{
    pid_t    gameId;
    enum MOVE move;
    char     tty[64]; // Terminal associé à la partie
} message;

typedef struct game_variable
{
    pid_t          gameId;
    int            grid[GRID_SIZE * GRID_SIZE];
    int            score;
    char           move;
    enum VALIDITY  validity;
    enum GAMESTATUS status;
    char           tty[64]; // Terminal associé à la partie
} game_variable;

typedef struct arg_moveAndScore
{
    game_variable *     gm;
    pthread_t           th_goal;
    pthread_mutex_t *   mut;
    sem_t *             sem_move;
    sem_t *             sem_goal;
} arg_moveAndScore;

typedef struct arg_goal
{
    game_variable       *gm;
    pthread_t           th_main;
    int                 fdDisplay;
    pthread_mutex_t     *mut;
    sem_t *             sem_goal;
    sem_t *             sem_main;
} arg_goal;

int   proc_2048(char *path);
void *func_moveAndScore(void *arg);
void *func_goal(void *arg);
