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

int running;

// Fonction pour stopper la boucle while dans le main
void stop_running(int sigrecu)
{
    running = 0;
}

// Fonction pour lire les entrées utilisateurs (https://stackoverflow.com/questions/421860/capture-characters-from-standard-input-without-waiting-for-enter-to-be-pressed)
char getch()
{
    char c;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt); // sauvegarde
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
    // Configuration du sigaction pour stopper le programme proprement à la réception de SIGUSR1
    struct sigaction sa_stop;

    sa_stop.sa_handler = stop_running;

    sigaction(SIGUSR1, &sa_stop, NULL);

    // Création du pipe nommé
    char *path = "/tmp/pipe_coup";

    if (mkfifo(path, 0666) == -1 && errno != EEXIST)
    {
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
    running = 1;
    while (running) // Se met sur 0 à la réception du SIGUSR1
    {
        char c = getch();
        enum MOVE m = NONE;
        printf("%d\n", c);
        if (c == 27)
        {
            if (getch() == '[')
            {
                switch (getch())
                {
                case 'A':
                    m = UP;
                    break;
                case 'B':
                    m = DOWN;
                    break;
                case 'C':
                    m = RIGHT;
                    break;
                case 'D':
                    m = LEFT;
                    break;
                }
            }
        }
        else if (c == 'q')
        {
            m = QUIT;
        }

        printf("%d\n", m);
        if (m != NONE)
            write(fd, &m, 1);
    }
    close(fd);    //
    unlink(path); // Suppression du pipe
    wait(NULL);   // Attente du fils (Moteur 2048)
    return EXIT_SUCCESS;
}
