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
    (void)sigrecu;
    running = 0;
}

// Lecture d'un caractère sans attendre Enter
char getch()
{
    char c = 0;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN]  = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    if (read(STDIN_FILENO, &c, 1) <= 0)
        c = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}

int main()
{
    char *path = "./pipe_move";

    if (access(path, F_OK) != 0)
    {
        // Pipe inexistant : cette instance est la première → crée proc_2048
        if (mkfifo(path, 0666) == -1 && errno != EEXIST)
        {
            perror("mkfifo");
            return EXIT_FAILURE;
        }

        pid_t pid = fork();
        CHKERR(pid);

        if (pid == 0) // Fils : processus 2048
        {
            return proc_2048(path);
        }
        else // Père : configuration des signaux
        {
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = stop_running;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGTERM, &sa, NULL);
            sigaction(SIGINT,  &sa, NULL);
        }
    }

    // Ouverture du pipe en écriture
    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        perror("open pipe");
        unlink(path);
        return EXIT_FAILURE;
    }

    // ── Envoi du message START avec le tty de cette instance ──
    message mStart;
    memset(&mStart, 0, sizeof(mStart));
    mStart.gameId = getpid();
    mStart.move   = START;

    char *ttyPath = ttyname(STDIN_FILENO); // ex: /dev/pts/3
    if (ttyPath)
        strncpy(mStart.tty, ttyPath, sizeof(mStart.tty) - 1);
    else
        strncpy(mStart.tty, "/dev/tty", sizeof(mStart.tty) - 1);

    ssize_t wStart = write(fd, &mStart, sizeof(mStart));
    if (wStart != sizeof(mStart))
        perror("write START");

    // ── Boucle de lecture des entrées ──
    running = 1;
    while (running)
    {
        char c = getch();

        message m;
        memset(&m, 0, sizeof(m));
        m.gameId = getpid();
        m.move   = NONE;

        if (c == 27) // Séquence flèche : ESC [ X
        {
            if (getch() == '[')
            {
                switch (getch())
                {
                    case 'A': m.move = UP;    break;
                    case 'B': m.move = DOWN;  break;
                    case 'C': m.move = RIGHT; break;
                    case 'D': m.move = LEFT;  break;
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
                    break;
                perror("write");
            }
        }
    }

    close(fd);
    unlink(path);
    return EXIT_SUCCESS;
}