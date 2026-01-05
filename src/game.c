#include "macro.h"
#include "game.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

/*
Fonction représentant le processus 2048
*/
void updateGameStatus(game_variable *gm);

int proc_2048(char * path) 
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
    game_variable * gm;
    CHKNULL(gm = calloc(1,sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));
    
    //int (*grid2D)[GRID_SIZE] = (int (*)[GRID_SIZE])grid; // Pointeur de manipulation [i][j]

    // Ouverture pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Création des threads
    pthread_t th_moveAndScore = 0, th_goal = 0;


    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, gm);
    pthread_create(&th_goal, NULL, func_goal, gm);

    // Thread Main

    char move;
    while (read(fdInput, &move, 1) > 0)
    {
        if (move == QUIT || gm->status != PROGRESS) // Gestion de la commande d'arrêt
        {
            kill(getppid(),SIGUSR1); // Envoie du SIGUSR1 au père (processus input)
            kill(pidDisplay,SIGUSR1); // Envoie du SIGUSR1 au processus display
            break;
        }
        // Envoie d'un signal à M&S pour traiter le coup
        pthread_kill(th_moveAndScore,SIGUSR2);
        // Attend un retour de goal       
    }

    // Libération
    free(gm->grid);
    free(gm);
    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);
    close(fdInput); // Fermeture du pipe nommé
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL); // Attente du fils (Display)
    return 0;
}

void *func_moveAndScore (void * arg) {
    game_variable * gm = (game_variable *)arg;


    return NULL;
}

void *func_goal (void * arg) {
    game_variable * gm = (game_variable *)arg;

    updateGameStatus(gm);
    
    //Envoi des infos à display via le pipe annonyme

    return NULL;
}

void updateGameStatus(game_variable *gm)
{
    bool hasEmptyCell = false;

    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        int cell = gm->grid[i];

        if (cell == OBJECTIV) {
            gm->status = WIN;
            return;
        }

        if (cell == 0) {
            hasEmptyCell = true;
        }
    }

    gm->status = hasEmptyCell ? PROGRESS : LOSE;
}