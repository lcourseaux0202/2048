#include "macro.h"
#include "game.h"
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

/*
Fonction représentant le processus 2048
*/
void updateGameStatus(game_variable *gm);
void addNumberOnGrid(int *grid);
void executeMove(int *grid, enum MOVE move, size_t size);
void processLine(int *line, size_t size);

void print_grid(int *grid); // Pour tester seulement

int proc_2048(char *path)
{
    srand(time(NULL));
    // Création du pipe annonyme pour l'affichage
    int fdDisplay[2];
    CHKERR(pipe(fdDisplay));

    pid_t pidDisplay = fork();

    if (pidDisplay == 0) // Processus fils
    {
        close(fdDisplay[1]); // Fermeture du pipe d'écriture

        // Lancement de la fonction d'affichage
        proc_display(fdDisplay[0]);
        printf("test\n");
        close(fdDisplay[0]); // Fermeture du pipe de lecture
        return 0;
    }
    // Processus père
    close(fdDisplay[0]); // Fermeture du pipe de lecture

    // Création des variables (pointeurs) pour la partie
    game_variable *gm;
    CHKNULL(gm = calloc(1, sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));

    // int (*grid2D)[GRID_SIZE] = (int (*)[GRID_SIZE])gm->grid; // Pointeur de manipulation [i][j]

    // Ouverture pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Blocage des signaux
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_MOVE);
    sigaddset(&set, SIG_GOAL);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIG_MAIN);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Création des threads
    pthread_t th_moveAndScore = 0, th_goal = 0;

    // Thread Goal
    arg_goal argGoal = {.gm = gm, .th_main = pthread_self(), .fdDisplay = fdDisplay[1]};
    pthread_create(&th_goal, NULL, func_goal, &argGoal);

    // Thread Move&Score
    arg_moveAndScore argMoveAndScore = {.gm = gm, .th_goal = th_goal};
    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, &argMoveAndScore);

    // Thread Main

    // Mise en place de la gestion des signaux
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIG_MAIN); // Affichage terminer

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Début du jeu
    addNumberOnGrid(gm->grid); // Ajout des deux premières cases
    addNumberOnGrid(gm->grid);

    enum MOVE move;
    while (read(fdInput, &move, sizeof(move)) == sizeof(move))
    {
        if (move == QUIT || gm->status != PROGRESS)
            break;

        gm->move = move;
        // Envoie d'un signal à M&S pour traiter le coup
        pthread_kill(th_moveAndScore, SIG_MOVE);

        // Attente de Goal et de display
        sigwait(&set, &sig); // Attend un signal
        sigwait(&set, &sig); // Attend un signal
    }

    // Arrêt des proc et threads
    pthread_kill(th_moveAndScore, SIGTERM);
    pthread_kill(th_goal, SIGTERM);
    kill(getppid(), SIGTERM);  // Envoie du SIGTERM au père (processus input)
    kill(pidDisplay, SIGTERM); // Envoie du SIGTERM au processus display

    // Libération
    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);
    free(gm->grid);
    free(gm);
    close(fdInput);      // Fermeture du pipe nommé
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL);          // Attente du fils (Display)
    return 0;
}

void *func_moveAndScore(void *arg)
{
    arg_moveAndScore *args = (arg_moveAndScore *)arg; // Cast des arguments
    game_variable *gm = args->gm;

    // Mise en place de la gestion des signaux
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIG_MOVE); // Nouveau move
    sigaddset(&set, SIGTERM);  // Arrêt

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    while (1)
    {
        sigwait(&set, &sig); // Attend un signal

        if (sig == SIGTERM) // Termine la boucle
            break;

        if (sig == SIG_MOVE) // Gère le move
        {
            executeMove(gm->grid, gm->move, GRID_SIZE);
            pthread_kill(args->th_goal, SIG_GOAL); // Passe la main à Goal
        }
    }

    return NULL;
}

void *func_goal(void *arg)
{
    arg_goal *args = (arg_goal *)arg; // Cast des arguments
    game_variable *gm = args->gm;

    // Mise en place de la gestion des signaux
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIG_GOAL); // Vérification de victoire
    sigaddset(&set, SIGTERM);  // Arrêt

    pthread_sigmask(SIG_BLOCK, &set, NULL);
    printf("2048\n");
    print_grid(gm->grid); // Affichage de départ
    printf("####\n");
    while (1)
    {
        sigwait(&set, &sig); // Attend un signal

        if (sig == SIGTERM) // Termine la boucle
            break;

        if (sig == SIG_GOAL) // Gère la condition de vitoire
        {
            updateGameStatus(gm);

            if (gm->status == PROGRESS)
            {
                addNumberOnGrid(gm->grid); // Ajout de la prochaine case
                printf("####\n");
            }

            write(args->fdDisplay, gm->grid, 16*sizeof(int));
            pthread_kill(args->th_main, SIG_MAIN); // Passe la main à Main
        }
    }

    return NULL;
}

void updateGameStatus(game_variable *gm)
{
    bool hasEmptyCell = false;

    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        int cell = gm->grid[i];

        if (cell == OBJECTIV)
        {
            gm->status = WIN;
            return;
        }

        if (cell == 0)
        {
            hasEmptyCell = true;
        }
        // Ajouter condition qu'on puisse encore faire une move
    }

    gm->status = hasEmptyCell ? PROGRESS : LOSE;
}

// Ajoute un nombre placé aléatoirment sur la grille (2 ou 4)
void addNumberOnGrid(int *grid)
{
    // Choix de l'emplacement
    int loc;
    do
    {
        loc = rand() % (GRID_SIZE * GRID_SIZE);
    } while (*(grid + loc) != 0);

    // Choix et placement de la valeur
    *(grid + loc) = rand() % 100 < 90 ? 2 : 4;
}

// Execute le move de l'utilisateur et retourne de score obtenu par les fusion (TODO : calculer le score)
void executeMove(int *grid, enum MOVE move, size_t size)
{
    int *line = malloc(size * sizeof(int));
    if (!line)
        return; // ou gérer l'erreur autrement

    for (size_t i = 0; i < size; i++)
    {
        if (move == LEFT || move == RIGHT)
        {
            // Extraction de la ligne
            for (size_t j = 0; j < size; j++)
            {
                size_t col = (move == LEFT) ? j : (size - 1 - j);
                line[j] = grid[i * size + col];
            }

            processLine(line, size);

            // Réécriture dans la grille
            for (size_t j = 0; j < size; j++)
            {
                size_t col = (move == LEFT) ? j : (size - 1 - j);
                grid[i * size + col] = line[j];
            }
        }
        else if (move == UP || move == DOWN)
        {
            // Extraction de la colonne
            for (size_t j = 0; j < size; j++)
            {
                size_t row = (move == UP) ? j : (size - 1 - j);
                line[j] = grid[row * size + i];
            }

            processLine(line, size);

            // Réécriture dans la grille
            for (size_t j = 0; j < size; j++)
            {
                size_t row = (move == UP) ? j : (size - 1 - j);
                grid[row * size + i] = line[j];
            }
        }
    }

    free(line);
}

// Fonction pour calculer le mouvement sur une ligne (les GRID_SIZEtuiles sont a,b,c,d)
void processLine(int *line, size_t size)
{
    int *temp = calloc(size, sizeof(int));
    int *finalLine = calloc(size, sizeof(int));

    if (!temp || !finalLine)
    {
        free(temp);
        free(finalLine);
        return;
    }

    int pos = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (line[i] != 0)
            temp[pos++] = line[i];
    }

    for (size_t i = 0; i + 1 < size; i++)
    {
        if (temp[i] != 0 && temp[i] == temp[i + 1])
        {
            temp[i] *= 2;
            temp[i + 1] = 0;
        }
    }

    pos = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (temp[i] != 0)
            finalLine[pos++] = temp[i];
    }

    for (size_t i = 0; i < size; i++)
        line[i] = finalLine[i];
    
    free(temp);
    free(finalLine);
}

// Affichage de la grille, test seulement, pourra être réutilisée pour display
void print_grid(int *grid)
{
    for (size_t i = 0; i < GRID_SIZE; i++)
    {
        for (size_t j = 0; j < GRID_SIZE; j++)
        {
            printf("%d ", grid[i * GRID_SIZE + j]);
        }
        printf("\n");
    }
}
