#include "macro.h"
#include "game.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

/*
Fonction représentant le processus 2048
*/
void updateGameStatus(game_variable *gm);

int proc_2048(char *path)
{
    // Création du pipe annonyme pour l'affichage
    int fdDisplay[2];
    CHKERR(pipe(fdDisplay));

    pid_t pidDisplay = fork();

    if (pidDisplay == 0) // Processus fils
    {
        close(fdDisplay[1]); // Fermeture du pipe d'écriture
        // Lancement de la fonction d'affichage

        close(fdDisplay[0]); // Fermeture du pipe de lecture
        return 0;
    }
    // Processus père
    close(fdDisplay[0]); // Fermeture du pipe de lecture

    // Création des variables (pointeurs) pour la partie
    game_variable *gm;
    CHKNULL(gm = calloc(1, sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));

    // int (*grid2D)[GRID_SIZE] = (int (*)[GRID_SIZE])grid; // Pointeur de manipulation [i][j]

    // Ouverture pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Création des threads
    pthread_t th_moveAndScore = 0, th_goal = 0;

    // Thread Goal
    arg_goal argGoal = {.gm = gm, .th_main = pthread_self(), .fdDisplay = fdDisplay[1]};
    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, &argGoal);

    // Thread Move&Score
    arg_moveAndScore argMoveAndScore = {.gm = gm, .th_goal = th_goal};
    pthread_create(&th_goal, NULL, func_goal, &argMoveAndScore);

    // Thread Main

    // Mise en place de la gestion des signaux
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1); // Affichage terminer

    enum MOVE move;
    while (read(fdInput, &move, 1) > 0)
    {
        if (move == QUIT || gm->status != PROGRESS) // Gestion de la commande d'arrêt
        {
            kill(getppid(), SIGTERM);  // Envoie du SIGTERM au père (processus input)
            kill(pidDisplay, SIGTERM); // Envoie du SIGTERM au processus display
            pthread_kill(th_moveAndScore, SIGTERM);
            pthread_kill(th_goal, SIGTERM);
            break;
        }
        // Envoie d'un signal à M&S pour traiter le coup
        pthread_kill(th_moveAndScore, SIGUSR1);

        // Attente de la fin de l'affichage
        sigwait(&set, &sig); // Attend un signal
    }

    // Libération
    free(gm->grid);
    free(gm);
    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);
    close(fdInput);      // Fermeture du pipe nommé
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL);          // Attente du fils (Display)
    return 0;
}

void *func_moveAndScore(void *arg)
{
    arg_moveAndScore *args = (arg_moveAndScore *)arg; // Cast des arguments
    game_variable *gm = args->gm;

    // Mise en place de la gestion des signaux
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1); // Nouveau move
    sigaddset(&set, SIGTERM); // Arrêt

    while (1)
    {
        sigwait(&set, &sig); // Attend un signal

        if (sig == SIGTERM) // Termine la boucle
            break;

        if (sig == SIGUSR1) // Gère le move
        {
            printf("Logique de move\n");

            pthread_kill(args->th_goal, SIGUSR1); // Passe la main à Goal
        }
    }

    return NULL;
}

void *func_goal(void *arg)
{
    arg_goal *args = (arg_goal *)arg; // Cast des arguments
    game_variable *gm = (game_variable *)args;

    // Mise en place de la gestion des signaux
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1); // Vérification de victoire
    sigaddset(&set, SIGTERM); // Arrêt

    while (1)
    {
        sigwait(&set, &sig); // Attend un signal

        if (sig == SIGTERM) // Termine la boucle
            break;

        if (sig == SIGUSR1) // Gère la condition de vitoire
        {
            printf("Logique de goal\n");
            updateGameStatus(gm);

            // Envoi des infos à display via le pipe annonyme
            pthread_kill(args->th_main, SIGUSR1); // Passe la main à Main
        }
    }

    return NULL;
}

void updateGameStatus(game_variable *gm)
{
    bool hasEmptyCell = false;

    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        int cell = gm->grid[i];

        if (cell == OBJECTIV)
        {
            gm->status = WIN;
            return;
        }

        if (cell == 0)
        {
            hasEmptyCell = true;
        }
    }

    gm->status = hasEmptyCell ? PROGRESS : LOSE;
}