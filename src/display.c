#include "../include/macro.h"
#include "../include/display.h"
#include "../include/signals.h"

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
    (void)sigrecu;
    displaying = 0;
}

int proc_display(int fdDisplay)
{
    displaying = 1;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = stop_display;
    sigaction(SIGTERM, &sa, NULL);

    game_variable *gm;
    CHKNULL(gm = calloc(1, sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));

    while (displaying)
    {
        if (read(fdDisplay, &gm->gameId, sizeof(pid_t)) != sizeof(pid_t)) break;
        if (read(fdDisplay, gm->tty,64) != 64) break;

        ssize_t nb = read(fdDisplay, gm->grid, 16 * sizeof(int));
        if (nb <= 0) break;

        read(fdDisplay, &gm->score,  sizeof(int));
        read(fdDisplay, &gm->status, sizeof(int));

        // Chaque partie a son propre tty (/dev/pts/N)
        FILE *out = fopen(gm->tty, "w");
        if (!out)
            out = stdout; // fallback si le tty est inaccessible

        fprintf(out, CLEAR); // Efface le terminal de cette instance
        fprintf(out, "\n\nScore : %d          Game ID : %d\n", gm->score, gm->gameId);
        fprintf(out, "|======||======||======||======|\n");

        for (size_t i = 0; i < GRID_SIZE; i++)
        {
            for (size_t j = 0; j < GRID_SIZE; j++)
            {
                int num = gm->grid[i * GRID_SIZE + j];
                switch (num)
                {
                    case 2:
                        fprintf(out, "|   " GREEN  "%d" DEFAULT "  |", num);
                        break;
                    case 4:
                        fprintf(out, "|   " YELLOW "%d" DEFAULT "  |", num);
                        break;
                    case 8:
                        fprintf(out, "|   " BLUE   "%d" DEFAULT "  |", num);
                        break;
                    case 16:
                        fprintf(out, "|  " PURPLE  "%d" DEFAULT "  |", num);
                        break;
                    case 32:
                       fprintf(out, "|  " CYAN    "%d" DEFAULT "  |", num);
                       break;
                    case 64:
                       fprintf(out, "|  " WHITE   "%d" DEFAULT "  |", num);
                       break;
                    case 128:
                       fprintf(out, "|  " RED     "%d" DEFAULT " |", num);
                       break;
                    case 256:
                       fprintf(out, "|  " GREEN   "%d" DEFAULT " |", num);
                       break;
                    case 512:
                       fprintf(out, "|  " YELLOW  "%d" DEFAULT " |", num);
                       break;
                    case 1024:
                       fprintf(out, "| " BLUE     "%d" DEFAULT " |", num);
                       break;
                    case 2048:
                       fprintf(out, "| " PURPLE   "%d" DEFAULT " |", num);
                       break;
                    default:
                       fprintf(out, "|      |");
                       break;
                }
            }
            fprintf(out, "\n|======||======||======||======|\n");
        }

        if (gm->status == LOSE)
        {
            fprintf(out, "You lose!\n");
        }

        if (gm->status == WIN)
        {
            fprintf(out, "You win!\n");
        }

        fflush(out);

        if (out != stdout)
            fclose(out); // Fermer le tty après chaque affichage

        kill(getppid(), SIG_MAIN);
    }

    free(gm->grid);
    free(gm);
    return 0;
}
