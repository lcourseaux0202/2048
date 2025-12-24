#include "macro.h"
#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <termios.h> // Utilisé dans la fonction getch()

// Fonction pour lire les entrées utilisateurs (https://stackoverflow.com/questions/421860/capture-characters-from-standard-input-without-waiting-for-enter-to-be-pressed)
char getch()
{
    char c;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);  // sauvegarde
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // non canonique, pas d'écho
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    read(STDIN_FILENO, &c, 1);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restauration
    return c;
}


int main()
{
    // Création du pipe nommé
    char *path = "/tmp/pipe_coup";

    if (mkfifo(path, 0666) == -1 && errno != EEXIST) { 
        perror("mkfifo"); 
        return EXIT_FAILURE; 
    }

    int fd;

    pid_t pid;
    CHKERR(pid = fork());

    if (pid == 0) // Processus fils
    {
        // Lancement du moteur 2048
        return proc_2048(path);
    }

    // Processus père
    CHKERR(fd = open(path, O_WRONLY));
    // Boucle des entrées utilisateurs
    while (1)
    {
        char c = getch();
        char dir = 0;
        if (c == 27)
        {
            getch();
            switch (getch())
            {
            case 'A':
                dir = 'u';
                break;
            case 'B':
                dir = 'd';
                break;
            case 'C':
                dir = 'r';
                break;
            case 'D':
                dir = 'l';
                break;
            }
        }
        else if (c == 'q')
            break;

        if (dir)
            write(fd, &dir, 1);
    }
    close(fd); // 
    unlink(path); // Suppression du pipe
    wait(NULL); // Attente du fils (Moteur 2048)
    return EXIT_SUCCESS;
}
