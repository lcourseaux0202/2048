#include "macro.h"
#include "game.h"
#include "display.h"
#include "signals.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

/* ── Prototypes internes ─────────────────────────────────────── */
int            addNewGame(game_variable **games, pid_t gameId, int gcount, char *tty);
game_variable *getGame(game_variable **games, pid_t gameId);
void           updateGameStatus(game_variable *gm);
void           addNumberOnGrid(int *grid);
enum VALIDITY  executeMove(int *grid, enum MOVE move, size_t size, int *score);
enum VALIDITY  processLine(int *line, size_t size, int *score);

/* ══════════════════════════════════════════════════════════════
 * proc_2048
 * ══════════════════════════════════════════════════════════════ */
int proc_2048(char *path)
{
    srand(time(NULL));

    // Pipe anonyme vers proc_display
    int fdDisplay[2];
    CHKERR(pipe(fdDisplay));

    pid_t pidDisplay = fork();

    if (pidDisplay == 0) // Fils : affichage
    {
        close(fdDisplay[1]);
        proc_display(fdDisplay[0]);
        close(fdDisplay[0]);
        return 0;
    }

    // Père : logique du jeu
    close(fdDisplay[0]);

    // Tableau des parties en cours
    game_variable *games[MAX_GAME_NB];
    memset(games, 0, sizeof(games));
    int gcount = 0;

    // Ouverture du pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Blocage des signaux
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_MOVE);
    sigaddset(&set, SIG_GOAL);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIG_MAIN);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Thread Goal
    game_variable *gm = NULL;
    arg_goal argGoal = {.gm = &gm, .th_main = pthread_self(), .fdDisplay = fdDisplay[1]};
    pthread_t th_goal = 0;
    pthread_create(&th_goal, NULL, func_goal, &argGoal);

    // Thread Move&Score
    arg_moveAndScore argMoveAndScore = {.gm = &gm, .th_goal = th_goal};
    pthread_t th_moveAndScore = 0;
    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, &argMoveAndScore);

    // Signaux attendus par le thread principal
    int sig;
    sigemptyset(&set);
    sigaddset(&set, SIG_MAIN);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // ── Boucle principale ──
    message m;
    while (read(fdInput, &m, sizeof(m)) == sizeof(m))
    {
        if (m.move == START)
        {
            // addNewGame reçoit maintenant le tty de l'instance
            if (addNewGame(games, m.gameId, gcount, m.tty))
                gcount++;
        }

        gm = getGame(games, m.gameId);
        if (gm == NULL)
            continue;

        if (m.move == QUIT || gm->status != PROGRESS)
            break;

        gm->move = m.move;

        // Signal à Move&Score
        pthread_kill(th_moveAndScore, SIG_MOVE);

        // Attente de Goal puis de Display
        sigwait(&set, &sig);
        sigwait(&set, &sig);
    }

    // Arrêt propre
    pthread_kill(th_moveAndScore, SIGTERM);
    pthread_kill(th_goal, SIGTERM);
    kill(getppid(), SIGTERM);
    kill(pidDisplay, SIGTERM);

    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);

    // Libération des parties
    for (int i = 0; i < gcount; i++)
    {
        if (games[i])
        {
            free(games[i]->grid);
            free(games[i]);
        }
    }

    close(fdInput);
    close(fdDisplay[1]);
    wait(NULL);
    return 0;
}

/* ── addNewGame : crée une nouvelle partie et stocke le tty ──── */
int addNewGame(game_variable **games, pid_t gameId, int gcount, char *tty)
{
    if (gcount >= MAX_GAME_NB)
        return 0;

    games[gcount] = calloc(1, sizeof(game_variable));
    if (!games[gcount]) return 0;

    games[gcount]->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int));
    if (!games[gcount]->grid) { free(games[gcount]); return 0; }

    games[gcount]->gameId = gameId;
    strncpy(games[gcount]->tty, tty, sizeof(games[gcount]->tty) - 1);

    addNumberOnGrid(games[gcount]->grid);
    addNumberOnGrid(games[gcount]->grid);

    return 1;
}

/* ── getGame ─────────────────────────────────────────────────── */
game_variable *getGame(game_variable **games, pid_t gameId)
{
    for (size_t i = 0; i < MAX_GAME_NB; i++)
    {
        if (games[i] && games[i]->gameId == gameId)
            return games[i];
    }
    return NULL;
}

/* ── func_moveAndScore ───────────────────────────────────────── */
void *func_moveAndScore(void *arg)
{
    arg_moveAndScore *args = (arg_moveAndScore *)arg;
    game_variable    *gm   = *(args->gm);

    sigset_t set;
    int      sig;
    sigemptyset(&set);
    sigaddset(&set, SIG_MOVE);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    while (1)
    {
        sigwait(&set, &sig);
        gm = *(args->gm);

        if (sig == SIGTERM)
            break;

        if (sig == SIG_MOVE)
        {
            gm->validity = executeMove(gm->grid, gm->move, GRID_SIZE, &gm->score);
            pthread_kill(args->th_goal, SIG_GOAL);
        }
    }
    return NULL;
}

/* ── func_goal ───────────────────────────────────────────────── */
void *func_goal(void *arg)
{
    arg_goal      *args = (arg_goal *)arg;
    game_variable *gm   = *(args->gm);

    sigset_t set;
    int      sig;
    sigemptyset(&set);
    sigaddset(&set, SIG_GOAL);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Envoi initial (grille vide au démarrage)
    // gm peut être NULL ici si aucune partie n'a encore démarré — on attend le premier SIG_GOAL

    while (1)
    {
        sigwait(&set, &sig);
        gm = *(args->gm);

        if (sig == SIGTERM)
            break;

        if (sig == SIG_GOAL)
        {
            updateGameStatus(gm);

            if (gm->status == PROGRESS && gm->validity == VALID)
                addNumberOnGrid(gm->grid);

            // Envoi au proc_display : gameId + tty + grille + score + status
            write(args->fdDisplay, &gm->gameId,  sizeof(pid_t));
            write(args->fdDisplay,  gm->tty,      64);
            write(args->fdDisplay,  gm->grid,     16 * sizeof(int));
            write(args->fdDisplay, &gm->score,    sizeof(int));
            write(args->fdDisplay, &gm->status,   sizeof(int));

            pthread_kill(args->th_main, SIG_MAIN);
        }
    }
    return NULL;
}

/* ── updateGameStatus ────────────────────────────────────────── */
void updateGameStatus(game_variable *gm)
{
    bool hasEmptyCell = false;
    bool canMerge     = false;

    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        int cell = gm->grid[i];

        if (cell == OBJECTIV)
        {
            gm->status = WIN;
            return;
        }
        if (cell == 0)
            hasEmptyCell = true;

        size_t col = i % GRID_SIZE;
        size_t row = i / GRID_SIZE;

        if (col + 1 < GRID_SIZE && cell == gm->grid[i + 1])
            canMerge = true;
        if (row + 1 < GRID_SIZE && cell == gm->grid[i + GRID_SIZE])
            canMerge = true;
    }

    gm->status = (hasEmptyCell || canMerge) ? PROGRESS : LOSE;
}

/* ── addNumberOnGrid ─────────────────────────────────────────── */
void addNumberOnGrid(int *grid)
{
    // Vérifier si la grille est pleine
    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
        if (grid[i] == 0) goto not_full;
    return;

not_full:;
    int loc;
    do {
        loc = rand() % (GRID_SIZE * GRID_SIZE);
    } while (grid[loc] != 0);

    grid[loc] = (rand() % 100 < 90) ? 2 : 4;
}

/* ── executeMove ─────────────────────────────────────────────── */
enum VALIDITY executeMove(int *grid, enum MOVE move, size_t size, int *score)
{
    enum VALIDITY validity = INVALID;

    int *line = malloc(size * sizeof(int));
    if (!line) return validity;

    for (size_t i = 0; i < size; i++)
    {
        if (move == LEFT || move == RIGHT)
        {
            for (size_t j = 0; j < size; j++)
            {
                size_t col = (move == LEFT) ? j : (size - 1 - j);
                line[j] = grid[i * size + col];
            }
            if (processLine(line, size, score) == VALID)
                validity = VALID;
            for (size_t j = 0; j < size; j++)
            {
                size_t col = (move == LEFT) ? j : (size - 1 - j);
                grid[i * size + col] = line[j];
            }
        }
        else if (move == UP || move == DOWN)
        {
            for (size_t j = 0; j < size; j++)
            {
                size_t row = (move == UP) ? j : (size - 1 - j);
                line[j] = grid[row * size + i];
            }
            if (processLine(line, size, score) == VALID)
                validity = VALID;
            for (size_t j = 0; j < size; j++)
            {
                size_t row = (move == UP) ? j : (size - 1 - j);
                grid[row * size + i] = line[j];
            }
        }
    }

    free(line);
    return validity;
}

/* ── processLine ─────────────────────────────────────────────── */
enum VALIDITY processLine(int *line, size_t size, int *score)
{
    int *temp      = calloc(size, sizeof(int));
    int *finalLine = calloc(size, sizeof(int));
    enum VALIDITY validity = INVALID;

    if (!temp || !finalLine)
    {
        free(temp);
        free(finalLine);
        return validity;
    }

    // Compacter
    int pos = 0;
    for (size_t i = 0; i < size; i++)
        if (line[i] != 0)
            temp[pos++] = line[i];

    // Fusionner
    for (size_t i = 0; i + 1 < size; i++)
    {
        if (temp[i] != 0 && temp[i] == temp[i + 1])
        {
            temp[i]   *= 2;
            *score    += temp[i];
            temp[i+1]  = 0;
        }
    }

    // Compacter après fusion
    pos = 0;
    for (size_t i = 0; i < size; i++)
        if (temp[i] != 0)
            finalLine[pos++] = temp[i];

    // Détecter changement
    for (size_t i = 0; i < size; i++)
    {
        if (line[i] != finalLine[i])
            validity = VALID;
        line[i] = finalLine[i];
    }

    free(temp);
    free(finalLine);
    return validity;
}