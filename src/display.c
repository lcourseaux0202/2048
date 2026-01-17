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

    game_variable *gm;
    CHKNULL(gm = calloc(1, sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));

    printf("\n\n            Start of the game!\n");
    while (displaying)
    {
        printf(CLEAR);      // clear le terminal
        ssize_t nb = read(fdDisplay, gm->grid, 16*sizeof(int));
        //ssize_t nb = read(fdDisplay, &gm, sizeof(gm));

        if (nb <= 0)
            break;

        read(fdDisplay, &gm->score, sizeof(int));
        read(fdDisplay, &gm->status, sizeof(int));


        printf("\n\nScore : %d\n", gm->score);
        printf("|======||======||======||======|\n");
        for (size_t i = 0; i < GRID_SIZE; i++)
        {
            for (size_t j = 0; j < GRID_SIZE; j++)
            {
                int num = gm->grid[i * GRID_SIZE + j];

                switch (num) {
                    case 2:
                        printf("|   " GREEN "%d" DEFAULT "  |", num);
                        break;
                    case 4:
                        printf("|   " YELLOW "%d" DEFAULT "  |", num);
                        break;
                    case 8:
                        printf("|   " BLUE "%d" DEFAULT "  |", num);
                        break;
                    case 16:
                        printf("|  " PURPLE "%d" DEFAULT "  |", num);
                        break;
                    case 32:
                        printf("|  " CYAN "%d" DEFAULT "  |", num);
                        break;
                    case 64:
                        printf("|  " WHITE "%d" DEFAULT "  |", num);
                        break;
                    case 128:
                        printf("|  " RED "%d" DEFAULT " |", num);
                        break;
                    case 256:
                        printf("|  " GREEN "%d" DEFAULT " |", num);
                        break;
                    case 512:
                        printf("|  " YELLOW "%d" DEFAULT " |", num);
                        break;
                    case 1024:
                        printf("| " BLUE "%d" DEFAULT " |", num);
                        break;
                    case 2048:
                        printf("| " PURPLE "%d" DEFAULT " |", num);
                        break;
                     default:
                        printf("|   %d  |", num);
                }
                /*if (num < 16)
                {
                    printf("|   \033[0;31m%d\033[0m  |", gm->grid[i * GRID_SIZE + j]);
                }
                else if (gm->grid[i * GRID_SIZE + j] < 128)
                {
                    printf("|  \033[0;32m%d\033[0m  |", gm->grid[i * GRID_SIZE + j]);
                }
                else if (gm->grid[i * GRID_SIZE + j] < 1024)
                {
                    printf("|  \033[0;33m%d\033[0m |", gm->grid[i * GRID_SIZE + j]);
                }
                else
                {
                    printf("| \033[0;34m%d\033[0m |", gm->grid[i * GRID_SIZE + j]);
                }*/

            }
            printf("\n|======||======||======||======|\n");
        }


        if (gm->status == LOSE) {
            printf("LOSERRRRRRRRRR!\n");
        }

        if (gm->status == WIN) {
            printf("WINNERRRRRRRRR!\n");
        }

        fflush(stdout);

        kill(getppid(), SIG_MAIN);
    }

    return 0;
}
