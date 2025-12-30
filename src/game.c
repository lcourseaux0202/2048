#include "macro.h"
#include "game.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

/*
Fonction représentant le processus 2048
*/

int proc_2048(char * path) 
{
    // Création du pipe annonyme pour l'affichage
    int fdDisplay[2];
    CHKERR(pipe(fdDisplay));
 
    pid_t pid = fork();

    if (pid == 0) // Processus fils
    {
        close(fdDisplay[1]); // Fermeture du pipe d'écriture
        // Lancement de la fonction d'affichage

        close(fdDisplay[0]); // Fermeture du pipe de lecture
        return 0;
    }
    // Processus père
    close(fdDisplay[0]); // Fermeture du pipe de lecture

    // Création de la grille
    int *grid; 
    CHKNULL(grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));
    
    //int (*grid2D)[GRID_SIZE] = (int (*)[GRID_SIZE])grid; // Pointeur de manipulation [i][j]

    // Ouverture pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Création des threads
    pthread_t th_moveAndScore = 0, th_goal = 0;

    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, grid);
    pthread_create(&th_goal, NULL, func_goal, grid);

    // Thread Main

    
    char move;
    while (read(fdInput, &move, 1) > 0)
    {
        if (move == QUIT)
        {
            kill();
            break;
        }
    }

    // Libération
    free(grid);
    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);
    close(fdInput); // Fermeture du pipe nommé
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL); // Attente du fils (Display)
    return 0;
}

void *func_moveAndScore (void * arg) {
    int * grid = (int *)arg;
    return NULL;
}

void *func_goal (void * arg) {
    int * grid = (int *)arg;
    return NULL;
}