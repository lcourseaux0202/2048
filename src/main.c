#include "macro.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <termios.h> // Utilisé dans la fonction getch()

// Fonction pour lire les entrées utilisateurs (https://stackoverflow.com/questions/421860/capture-characters-from-standard-input-without-waiting-for-enter-to-be-pressed)
char getch()
{
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return (buf);
}

int main()
{
    char *path = "/tmp/pipe_coup";
    mkfifo(path, 0666);
    int fd;

    if (fork() == 0)
    {
        // fils : processus 2048
        fd = open(path, O_RDONLY);
        char c;
        while (read(fd, &c, 1) > 0)
        {
            printf("\nMouvement : %c", c);
        }
        exit(EXIT_SUCCESS);
    }
    else
    {
        // père : entrées utilisateurs
        fd = open(path, O_WRONLY);
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
        close(fd);
        unlink(path);
    }
    return 0;
}
