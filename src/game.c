#include "macro.h"
#include "game.h"

#include <unistd.h>

int proc_2048(int fd) 
{
    char c;
    while (read(fd, &c, 1) > 0)
    {
        printf("Mouvement : %c\n", c);
    }
    return 0;
}