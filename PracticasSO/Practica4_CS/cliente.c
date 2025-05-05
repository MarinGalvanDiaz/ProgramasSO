#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#define SHM_SIZE 1024

typedef struct {
    sem_t semServidor;
    sem_t semCliente;
    char mensaje[256];
} memCompartida;

int cuentaPalabras(const char *cadena) {
    int espacios = 0;
    for (int i = 0; cadena[i] != '\0'; i++) {
        if (cadena[i] == ' ') {
            espacios++;
        }
    }
    return espacios == 9;
}

int main() {
    FILE *f;
    if ((f = fopen("memfile", "a")) == NULL) {
        perror("fopen memfile");
        exit(1);
    }
    fclose(f);

    key_t claveShm = ftok("memfile", 'M');
    if (claveShm == -1) {
        perror("ftok");
        exit(1);
    }

    int shmid = shmget(claveShm, sizeof(memCompartida), 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    memCompartida *datosMC = (memCompartida *)shmat(shmid, NULL, 0);
    if (datosMC == (memCompartida *)-1) {
        perror("shmat");
        exit(1);
    }

    char frase[512];

    printf("----- CLIENTE con PID: %d -----\n", getpid());

    do {
        printf("Ingrese una frase de 10 palabras: ");
        if (!fgets(frase, sizeof(frase), stdin)) {
            perror("fgets");
            shmdt(datosMC);
            exit(1);
        }

        frase[strcspn(frase, "\n")] = '\0';

        if (!cuentaPalabras(frase)) {
            printf("Error: la frase debe contener exactamente 10 palabras. Intente nuevamente\n");
        }

    } while (!cuentaPalabras(frase));

    char inicFrase[64];
    snprintf(inicFrase, sizeof(inicFrase), "PID %d: ", getpid());

    strncpy((*datosMC).mensaje, inicFrase, sizeof((*datosMC).mensaje) - 1);
    (*datosMC).mensaje[sizeof((*datosMC).mensaje) - 1] = '\0';

    size_t espacioDisp = sizeof((*datosMC).mensaje) - strlen((*datosMC).mensaje) - 1;

    strncat((*datosMC).mensaje, frase, espacioDisp);

    sem_post(&(*datosMC).semCliente);
    sem_wait(&(*datosMC).semServidor);

    printf("Mensaje enviado correctamente al servidor\n");

    shmdt(datosMC);
    return 0;
}
