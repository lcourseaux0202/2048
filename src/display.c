#include "macro.h"
#include "display.h"
#include "signals.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

int displaying;

void stop_display(int sigrecu)
{
    displaying = 0;
}

int proc_display(int fdDisplay)
{
    displaying = 1;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = stop_display;
    sigaction(SIGTERM, &sa, NULL);

    int buffer[1024];

    while (displaying)
    {
        ssize_t nb = read(fdDisplay, buffer, 16*sizeof(int));

        if (nb <= 0)
            break;

        printf("\n\n|======||======||======||======|\n");
        for (size_t i = 0; i < GRID_SIZE; i++)
        {
            for (size_t j = 0; j < GRID_SIZE; j++)
            {
                if (buffer[i * GRID_SIZE + j] < 16)
                {
                    printf("|   %d  |", buffer[i * GRID_SIZE + j]);
                }
                else if (buffer[i * GRID_SIZE + j] < 128)
                {
                    printf("|  %d  |", buffer[i * GRID_SIZE + j]);
                }
                else if (buffer[i * GRID_SIZE + j] < 1024)
                {
                    printf("|  %d |", buffer[i * GRID_SIZE + j]);
                }
                else
                {
                    printf("| %d |", buffer[i * GRID_SIZE + j]);
                }

            }
            printf("\n|======||======||======||======|\n");
        }

        fflush(stdout);

        kill(getppid(), SIG_MAIN);
    }

    return 0;
}
