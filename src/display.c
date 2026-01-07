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

    char buffer[1024];

    while (displaying)
    {
        ssize_t nb = read(fdDisplay, buffer, sizeof(buffer) - 1);

        if (nb <= 0)
            break;

        buffer[nb] = '\0';
        printf("%s", buffer);
        fflush(stdout);

        //kill(getppid(), SIG_MAIN);
    }

    return 0;
}
