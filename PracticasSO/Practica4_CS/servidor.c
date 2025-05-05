#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#define SHM_SIZE 1024

typedef struct {
    sem_t semServidor;
    sem_t semCliente;
    char mensaje[256];
} memCompartida;

typedef struct {
    char mensaje[256];
} argsHilo;

int shmid;
memCompartida *datosMC;
volatile sig_atomic_t servActivo = 1;

void liberaRecursos() {
    printf("\nLiberando recursos...\n");
    shmdt(datosMC);
    shmctl(shmid, IPC_RMID, NULL);
    printf("Recursos liberados. Saliendo del servidor\n");
    exit(0);
}

void senialTerminar(int sig) {
    if (sig == SIGINT) {
        printf("\nRecibida señal de fin. Terminando servidor...\n");
        servActivo = 0;
        sem_post(&(*datosMC).semCliente);
    }
}

void *atiendeCliente(void *arg) {
    argsHilo *args = (argsHilo *)arg;
    pthread_t tid = pthread_self();

    char msjLocal[256];
    int contador = 0;

    strncpy(msjLocal, (*args).mensaje, sizeof(msjLocal) - 1);
    msjLocal[sizeof(msjLocal) - 1] = '\0';

    free(args);

    usleep(1000);

    printf("Hilo con TID %lu: Mensaje recibido: %s\n",
           (unsigned long)tid, msjLocal);

    pthread_exit(NULL);
}

int main() {
    signal(SIGINT, senialTerminar);
    atexit(liberaRecursos);

    FILE *f;
    if ((f = fopen("memfile", "a")) == NULL) {
        perror("fopen memfile");
        exit(1);
    }
    fclose(f);

    key_t claveShm = ftok("memfile", 'M');

    printf("-----SERVIDOR-----\n");
    printf("Presione 'Ctrl+C' para terminar el servidor cuando desee\n");

    shmid = shmget(claveShm, sizeof(memCompartida), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    datosMC = (memCompartida *)shmat(shmid, NULL, 0);
    if (datosMC == (memCompartida *)-1) {
        perror("shmat");
        exit(1);
    }

    static int inicializado = 0;
    if (!inicializado) {
        sem_init(&(*datosMC).semServidor, 1, 0);
        sem_init(&(*datosMC).semCliente, 1, 0);
        inicializado = 1;
    }

    while (servActivo) {
        printf("Servidor esperando mensaje...\n");

        sem_wait(&(*datosMC).semCliente);
        if (!servActivo) break;

        argsHilo *args = malloc(sizeof(argsHilo));
        if (!args) {
            perror("malloc");
            continue;
        }

        strncpy((*args).mensaje, (*datosMC).mensaje, sizeof((*args).mensaje) - 1);
        (*args).mensaje[sizeof((*args).mensaje) - 1] = '\0';

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atiendeCliente, (void *)args) != 0) {
            perror("pthread_create");
            free(args);
            continue;
        }

        sem_post(&(*datosMC).semServidor);
    }

    return 0;
}
