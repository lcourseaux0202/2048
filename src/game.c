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

void addNewGame();
game_variable *getGame(game_variable **games, pid_t gameId);
void updateGameStatus(game_variable *gm);
void addNumberOnGrid(int *grid);
enum VALIDITY executeMove(int *grid, enum MOVE move, size_t size, int *score);
enum VALIDITY processLine(int *line, size_t size, int *score);

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
        close(fdDisplay[0]); // Fermeture du pipe de lecture
        return 0;
    }
    // Processus père
    close(fdDisplay[0]); // Fermeture du pipe de lecture

    // Création des variables (pointeurs) pour la partie
    game_variable *games[8];
    game_variable *gm;
    CHKNULL(gm = calloc(1, sizeof(game_variable)));
    CHKNULL(gm->grid = calloc(GRID_SIZE * GRID_SIZE, sizeof(int)));

    games[0] = gm;

    // int (*grid2D)[GRID_SIZE] = (int (*)[GRID_SIZE])gm->grid; // Pointeur de manipulation [i][j]
    addNumberOnGrid(games[0]->grid); // Ajout des deux premières cases
    addNumberOnGrid(games[0]->grid);

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

    message m;
    while (read(fdInput, &m, sizeof(m)) == sizeof(m))
    {
        if (m.move == START)
        {
            games[0]->gameId = m.gameId; // à remplacer par une fonction qui ajoute une nouvelle partie
        }

        gm = getGame(games, m.gameId);
        if (gm != NULL)
        {
            if (m.move == QUIT || gm->status != PROGRESS)
                break;

            gm->move = m.move;
            // Envoie d'un signal à M&S pour traiter le coup

            pthread_kill(th_moveAndScore, SIG_MOVE);

            // Attente de Goal et de display
            sigwait(&set, &sig); // Attend un signal
            sigwait(&set, &sig); // Attend un signal
        }
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

void addNewGame() {}

game_variable *getGame(game_variable **games, pid_t gameId)
{
    for (size_t i = 0; i < 8; i++)
    {
        if (games[i] && games[i]->gameId == gameId)
        {
            return games[i];
        }
    }
    return NULL;
}

void *func_moveAndScore(void *arg)
{
    arg_moveAndScore *args = (arg_moveAndScore *)arg; // Cast des arguments
    game_variable *gm = args->gm;
    gm->validity = VALID;

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
            gm->validity = executeMove(gm->grid, gm->move, GRID_SIZE, &gm->score);
            // printf("Temp Score : %d\n", gm->score);
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

    write(args->fdDisplay, &gm->gameId, sizeof(pid_t));
    write(args->fdDisplay, gm->grid, 16 * sizeof(int));
    write(args->fdDisplay, &gm->score, sizeof(int));
    write(args->fdDisplay, &gm->status, sizeof(int));

    while (1)
    {
        sigwait(&set, &sig); // Attend un signal

        if (sig == SIGTERM) // Termine la boucle
            break;

        if (sig == SIG_GOAL) // Gère la condition de vitoire
        {
            updateGameStatus(gm);

            if (gm->status == PROGRESS && gm->validity == VALID)
            {
                addNumberOnGrid(gm->grid); // Ajout de la prochaine case
            }

            write(args->fdDisplay, &gm->gameId, sizeof(pid_t));
            write(args->fdDisplay, gm->grid, 16 * sizeof(int));
            write(args->fdDisplay, &gm->score, sizeof(int));
            write(args->fdDisplay, &gm->status, sizeof(int));
            pthread_kill(args->th_main, SIG_MAIN); // Passe la main à Main
        }
    }

    return NULL;
}

void updateGameStatus(game_variable *gm)
{
    bool hasEmptyCell = false;
    bool canMerge = false;

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

        // Vérifications si fusion possible
        size_t col = i % GRID_SIZE;
        size_t row = i / GRID_SIZE;

        if (col + 1 < GRID_SIZE && cell == gm->grid[i + 1])
        {
            canMerge = true;
        }

        if (row + 1 < GRID_SIZE && cell == gm->grid[i + GRID_SIZE])
        {
            canMerge = true;
        }
    }

    gm->status = (hasEmptyCell || canMerge) ? PROGRESS : LOSE;
}

// Ajoute un nombre placé aléatoirment sur la grille (2 ou 4)
void addNumberOnGrid(int *grid)
{
    // Pour quand la grille est pleine, mais que des mouvs sont encore possibles
    int testGridFull = 1;
    for (size_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        if (grid[i] == 0)
        {
            testGridFull = 0;
            break;
        }
    }
    if (testGridFull == 1)
    {
        return;
    }

    // Choix de l'emplacement
    int loc;
    do
    {
        loc = rand() % (GRID_SIZE * GRID_SIZE);
    } while (*(grid + loc) != 0);

    // Choix et placement de la valeur
    *(grid + loc) = rand() % 100 < 90 ? 2 : 4;
}

// Execute le move de l'utilisateur et retourne de score obtenu par les fusion
enum VALIDITY executeMove(int *grid, enum MOVE move, size_t size, int *score)
{
    enum VALIDITY validity = INVALID;

    int *line = malloc(size * sizeof(int));
    if (!line)
        return validity;

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

            if (processLine(line, size, score) == VALID)
            {
                validity = VALID;
            }

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

            if (processLine(line, size, score) == VALID)
            {
                validity = VALID;
            }

            // Réécriture dans la grille
            for (size_t j = 0; j < size; j++)
            {
                size_t row = (move == UP) ? j : (size - 1 - j);
                grid[row * size + i] = line[j];
            }
        }
    }

    free(line);
    return validity;
}

// Fonction pour calculer le mouvement sur une ligne (les GRID_SIZEtuiles sont a,b,c,d)
enum VALIDITY processLine(int *line, size_t size, int *score)
{
    int *temp = calloc(size, sizeof(int));
    int *finalLine = calloc(size, sizeof(int));

    enum VALIDITY validity = INVALID;

    if (!temp || !finalLine)
    {
        free(temp);
        free(finalLine);
        return validity;
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
            *score += temp[i]; // On ajoute au score la valeur de la tuiles crée
        }
    }

    pos = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (temp[i] != 0)
            finalLine[pos++] = temp[i];
    }

    for (size_t i = 0; i < size; i++)
    {
        if (line[i] != finalLine[i])
        {
            validity = VALID;
        }
        line[i] = finalLine[i];
    }

    free(temp);
    free(finalLine);

    return validity;
}
