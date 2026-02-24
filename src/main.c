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

#include <termios.h>

int running;

// Fonction pour stopper la boucle while dans le main
void stop_running(int sigrecu)
{
    (void)sigrecu; // éviter warning unused
    running = 0;
}

// Fonction pour lire les entrées utilisateurs sans attendre Enter
char getch()
{
    char c = 0;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt); // sauvegarde
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // non canonique, pas d'écho
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (read(STDIN_FILENO, &c, 1) <= 0)
        c = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restauration
    return c;
}

int main()
{
    char *path = "./pipe_move";

    if (access(path, F_OK) != 0) // https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
    {
        // Si le pipe n'existe pas : création du pipe nommé + lancement de proc_2048
        if (mkfifo(path, 0666) == -1 && errno != EEXIST)
        {
            perror("mkfifo");
            return EXIT_FAILURE;
        }

        pid_t pid = fork();
        CHKERR(pid);

        if (pid == 0) // Processus fils
        {
            return proc_2048(path);
        }
        else // Processus père
        {
            // Configuration du sigaction pour stopper le programme proprement
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = stop_running;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGTERM, &sa, NULL);
            sigaction(SIGINT, &sa, NULL);
        }
    }

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        perror("open pipe");
        // kill(pid, SIGTERM);
        unlink(path);
        return EXIT_FAILURE;
    }

    message mStart;
    mStart.gameId = getpid();
    mStart.move = START;

    ssize_t wStart = write(fd, &mStart, sizeof(mStart));
    if (wStart != sizeof(mStart))
    {
        perror("write");
    }

    running = 1;
    while (running)
    {
        char c = getch();
        message m;
        m.gameId = getpid();
        m.move = NONE;

        if (c == 27) // flèches
        {
            if (getch() == '[')
            {
                switch (getch())
                {
                case 'A':
                    m.move = UP;
                    break;
                case 'B':
                    m.move = DOWN;
                    break;
                case 'C':
                    m.move = RIGHT;
                    break;
                case 'D':
                    m.move = LEFT;
                    break;
                }
            }
        }
        else if (c == 'q')
        {
            m.move = QUIT;
        }

        if (m.move != NONE)
        {
            ssize_t w = write(fd, &m, sizeof(m));
            if (w != sizeof(m))
            {
                if (errno == EPIPE)
                    break; // pipe fermé
                perror("write");
            }
        }
    }
    close(fd);    // fermeture du pipe
    unlink(path); // Suppression du pipe
    return EXIT_SUCCESS;
}
