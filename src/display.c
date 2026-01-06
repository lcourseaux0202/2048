#include "macro.h"
#include "display.h"

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

    struct sigaction sa_stop, sa_term;

    sa_stop.sa_handler = stop_display;
    sa_term.sa_handler = stop_display;

    sigaction(SIGUSR1, &sa_stop, NULL);
    sigaction(SIGTERM, &sa_term, NULL);

    char buffer[1024];

    while (displaying)
    {
        read(fdDisplay, buffer, 1024);
        printf(buffer);
    }
    return 0;
}