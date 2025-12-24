#include "macro.h"
#include "game.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

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

    // Ouverture pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    char c;
    while (read(fdInput, &c, 1) > 0)
    {
        printf("Mouvement : %c\n", c);
    }

    close(fdInput); // Fermeture du pipe nommé
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL); // Attente du fils (Display)
    return 0;
}