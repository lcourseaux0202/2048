#include "../include/macro.h"
#include "../include/game.h"
#include "../include/display.h"
#include "../include/signals.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/shm.h>

/*
Fonction représentant le processus 2048
*/

int addNewGame(game_variable **games, pid_t gameId, int gcount, char *tty);
game_variable *getGame(game_variable **games, pid_t gameId);
void updateGameStatus(game_variable *gm);
void addNumberOnGrid(int *grid);
enum VALIDITY executeMove(int *grid, enum MOVE move, size_t size, int *score);
enum VALIDITY processLine(int *line, size_t size, int *score);

int shmid;

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
    game_variable *games[MAX_GAME_NB] = {NULL};
    int gcount = 0;

    game_variable *gm;

    // Création du segment de mémoire partagé
    key_t key = ftok(SEGMENT_PATH, PROJECT_ID);                       // Clé
    int shmid = shmget(key, sizeof(game_variable), IPC_CREAT | 0664); // Création du segment
    if (shmid == -1)
    {
        shmid = shmget(key, sizeof(game_variable), 0664);
        if (shmid != -1)
            shmctl(shmid, IPC_RMID, NULL);
        shmid = shmget(key, sizeof(game_variable), IPC_CREAT | 0664);
    }
    game_variable *sharredGame = shmat(shmid, (void *)0, 0); // Attachement au segment

    // Création du mutex
    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

    // Création des sémaphores
    sem_t sem_move;
    sem_t sem_goal;
    sem_t sem_main;
    sem_init(&sem_move, 0, 0);
    sem_init(&sem_goal, 0, 0);
    sem_init(&sem_main, 0, 0);

    // Ouverture du pipe nommé
    int fdInput;
    CHKERR(fdInput = open(path, O_RDONLY));

    // Blocage des signaux
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Création des threads
    pthread_t th_moveAndScore = 0, th_goal = 0;

    // Thread Goal
    arg_goal argGoal = {.gm = sharredGame, .th_main = pthread_self(), .fdDisplay = fdDisplay[1], .mut = &mut, .sem_goal = &sem_goal, .sem_main = &sem_main};
    pthread_create(&th_goal, NULL, func_goal, &argGoal);

    // Thread Move&Score
    arg_moveAndScore argMoveAndScore = {.gm = sharredGame, .th_goal = th_goal, .mut = &mut, .sem_move = &sem_move, .sem_goal = &sem_goal};
    pthread_create(&th_moveAndScore, NULL, func_moveAndScore, &argMoveAndScore);

    // Thread Main

    // Mise en place de la gestion des signaux
    int sig;

    sigemptyset(&set);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    printf("Lancement de la boucle principale\n");
    message m;
    while (read(fdInput, &m, sizeof(m)) == sizeof(m))
    {
        // Création d'une partie si nécessaire
        if (m.move == START)
        {
            if (addNewGame(games, m.gameId, gcount, m.tty))
                gcount++;
        }

        gm = getGame(games, m.gameId);
        if (gm != NULL)
        {
            if (m.move == QUIT || gm->status != PROGRESS)
            {
                kill(m.gameId, SIGTERM);

                // Recherche de la partie à libérer
                for (int i = 0; i < MAX_GAME_NB; i++)
                {
                    if (games[i] && games[i]->gameId == m.gameId)
                    {
                        free(games[i]);
                        games[i] = NULL;
                        gcount--;
                        break;
                    }
                }

                printf("Suppression d'une partie, gcount = %d\n", gcount);

                if (gcount > 0)
                    continue;
                break;
            }
            gm->move = m.move;

            // Copie de la partie dans shm
            pthread_mutex_lock(&mut);
            memcpy(sharredGame, gm, sizeof(game_variable));
            pthread_mutex_unlock(&mut);
            sem_post(&sem_move);

            // Recopie de la partie du shm
            sem_wait(&sem_main);
            pthread_mutex_lock(&mut);
            memcpy(gm, sharredGame, sizeof(game_variable));
            pthread_mutex_unlock(&mut);
        }
    }
    printf("Terminaison du programme\n");

    printf("Libération des sémaphores\n");
    sem_post(&sem_move);
    sem_post(&sem_goal);
    sem_post(&sem_main);



    pthread_cancel(th_moveAndScore);
    pthread_cancel(th_goal);
    pthread_join(th_moveAndScore, NULL);
    pthread_join(th_goal, NULL);


    // Arrêt propre
    //pthread_kill(th_moveAndScore, SIGTERM);
    //pthread_kill(th_goal, SIGTERM);
    kill(pidDisplay, SIGTERM);

    // printf("Threads en attente\n");
    //
    // pthread_join(th_moveAndScore, NULL);
    // pthread_join(th_goal, NULL);
    //
    // printf("Threads récupérés\n");

    // Libération des parties
    printf("Libération des parties\n");
    for (int i = 0; i < gcount; i++)
    {
        if (games[i])
            free(games[i]);
    }

    // Fermeture du segment de mémoire partagé
    printf("Fermeture du segment de mémoire partagé\n");
    shmdt(sharredGame);
    shmctl(shmid, IPC_RMID, NULL);

    // Suppression du mutex
    printf("Destruction du mutex\n");
    pthread_mutex_destroy(&mut);

    // Suppression des sémapores
    printf("Destruction des sémaphores\n");
    sem_destroy(&sem_move);
    sem_destroy(&sem_goal);
    sem_destroy(&sem_main);


    printf("Fermeture du pipe d'affichage\n");
    close(fdDisplay[1]); // Fermeture du pipe d'écriture
    wait(NULL);          // Attente du fils (Display)


    printf("Fermeture du pipe nommé\n");
    close(fdInput); // Fermeture du pipe nommé
    printf("Suppression du pipe nommé\n");
    unlink(path); // Suppression du pipe
    printf("Pipe nommé supprimé\n");

    printf("Tout supprimé\n");
    return 0;
}

int addNewGame(game_variable **games, pid_t gameId, int gcount, char *tty)
{
    if (gcount >= MAX_GAME_NB)
        return 0;

    // Cherche le premier slot libre
    int slot = -1;
    for (int i = 0; i < MAX_GAME_NB; i++)
    {
        if (games[i] == NULL)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return 0;

    games[slot] = calloc(1, sizeof(game_variable));
    games[slot]->gameId = gameId;
    games[slot]->score = 0;
    games[slot]->status = PROGRESS;
    strncpy(games[slot]->tty, tty, sizeof(games[slot]->tty) - 1);
    addNumberOnGrid(games[slot]->grid);
    addNumberOnGrid(games[slot]->grid);

    return 1;
}

game_variable *getGame(game_variable **games, pid_t gameId)
{
    for (size_t i = 0; i < MAX_GAME_NB; i++)
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
    sem_t *sem_move = args->sem_move;
    sem_t *sem_goal = args->sem_goal;
    pthread_mutex_t *mut = args->mut;
    game_variable *gm = args->gm;
    gm->validity = VALID;

    // Mise en place de la gestion des signaux
    // sigset_t set;
    // int sig;
    // sigemptyset(&set);
    // sigaddset(&set, SIGTERM); // Arrêt

    // pthread_sigmask(SIG_BLOCK, &set, NULL); // remplacer par sigaction

    while (1)
    {
        // sigwait(&set, &sig); // Attend un signal

        // if (sig == SIGTERM) // Termine la boucle
        //     break;

        // if (sig == SIG_MOVE) // Gère le move

        sem_wait(sem_move);
        pthread_mutex_lock(mut);
        gm->validity = executeMove(gm->grid, gm->move, GRID_SIZE, &gm->score);
        pthread_mutex_unlock(mut);
        sem_post(sem_goal);
    }

    return NULL;
}

void *func_goal(void *arg)
{
    arg_goal *args = (arg_goal *)arg; // Cast des arguments
    game_variable *gm = args->gm;
    sem_t *sem_goal = args->sem_goal;
    sem_t *sem_main = args->sem_main;
    pthread_mutex_t *mut = args->mut;
    // Mise en place de la gestion des signaux

    // sigset_t set;
    // int sig;
    // sigemptyset(&set);
    // sigaddset(&set, SIGTERM); // Arrêt

    // pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_mutex_lock(mut);
    // Envoi au proc_display : gameId + tty + grille + score + status
    write(args->fdDisplay, &gm->gameId, sizeof(pid_t));
    write(args->fdDisplay, gm->tty, 64);
    write(args->fdDisplay, gm->grid, 16 * sizeof(int));
    write(args->fdDisplay, &gm->score, sizeof(int));
    write(args->fdDisplay, &gm->status, sizeof(int));

    pthread_mutex_unlock(mut);
    while (1)
    {
        // sigwait(&set, &sig); // Attend un signal

        // if (sig == SIGTERM) // Termine la boucle
        //    break;

        sem_wait(sem_goal);
        pthread_mutex_lock(mut);
        updateGameStatus(gm);

        if (gm->status == PROGRESS && gm->validity == VALID)
        {
            addNumberOnGrid(gm->grid); // Ajout de la prochaine case
        }

        // Envoi au proc_display : gameId + tty + grille + score + status
        write(args->fdDisplay, &gm->gameId, sizeof(pid_t));
        write(args->fdDisplay, gm->tty, 64);
        write(args->fdDisplay, gm->grid, 16 * sizeof(int));
        write(args->fdDisplay, &gm->score, sizeof(int));
        write(args->fdDisplay, &gm->status, sizeof(int));

        pthread_mutex_unlock(mut);

        sem_post(sem_main);
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
