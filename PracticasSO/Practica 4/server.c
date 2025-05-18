//bibliotecas
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

// mesaje del cliente y pid del cliente
typedef struct{
    char mensaje[256];
    pid_t pidCliente;
} DatosCliente;

// memoria compartida; semaforos, datos del cliente y clave de memoria de cada cliente
typedef struct{
    sem_t semServidor;
    sem_t semCliente;
    DatosCliente datos;
    key_t claveCliente;
} MemoriaCompartida;


int shmid_global; // memoria compartida global 
MemoriaCompartida *datosCompartidos; // apunta a memoria compartida
volatile sig_atomic_t servidorActivo = 1; // termina el servidor activo
int contadorClientes = 1; // para que cada cliente tenga clave unica 
pthread_mutex_t mutexContador = PTHREAD_MUTEX_INITIALIZER; // hace que el contador evite condiciones de carrera

// libera recursos correctamente al finalizar (se ayuda de el manejo de señales)
void liberarRecursos(){
    printf("\nLiberando recursos...\n");
    sem_destroy(&((*datosCompartidos).semServidor));
    sem_destroy(&((*datosCompartidos).semCliente));
    shmdt(datosCompartidos);
    shmctl(shmid_global, IPC_RMID, NULL);
    pthread_mutex_destroy(&mutexContador);
    printf("Recursos liberados. Saliendo del servidor\n");
    exit(0);
}

// cuando se da ctrl c, cambia servidor activo
void manejarSenial(int sig){
    if (sig == SIGINT)
    {
        printf("\nSeñal de terminación recibida. Apagando servidor...\n");
        servidorActivo = 0;
        sem_post(&(*datosCompartidos).semCliente);
    }
}

// hilo por cada cliente, muestra mensaje
void *atencionCliente(void *arg){
    MemoriaCompartida *memHilo = (MemoriaCompartida *)arg;
    printf("\n--- Hilo atendiendo cliente PID: %d ---\n", (*memHilo).datos.pidCliente);
    printf("Mensaje recibido: %s\n", (*memHilo).datos.mensaje);

    int shmid = shmget((*memHilo).claveCliente, 0, 0);
    shmdt(memHilo);
    if (shmid >= 0){
        shmctl(shmid, IPC_RMID, NULL);
    }
        
    pthread_exit(NULL);
}

// principal
int main(){
    signal(SIGINT, manejarSenial);
    atexit(liberarRecursos);

    // genera clave
    FILE *f = fopen("memfile", "a");
    if (f == NULL){
        perror("fopen memfile");
        exit(1);
    }
    fclose(f);

    key_t claveShm = ftok("memfile", 'M');
    if (claveShm == -1){
        perror("ftok");
        exit(1);
    }

    // crea memoria compartida principal
    shmid_global = shmget(claveShm, sizeof(MemoriaCompartida), IPC_CREAT | 0666);
    if (shmid_global < 0){
        perror("shmget");
        exit(1);
    }
    datosCompartidos = (MemoriaCompartida *)shmat(shmid_global, NULL, 0);
    if (datosCompartidos == (MemoriaCompartida *)-1){
        perror("shmat");
        exit(1);
    }

    // inicializa semaforos
    sem_init(&(*datosCompartidos).semServidor, 1, 0);
    sem_init(&(*datosCompartidos).semCliente, 1, 0);

    printf("----- SERVIDOR INICIADO -----\n");
    printf("Presione Ctrl+C para terminar el servidor\n\n");

    // espera un cliente, crea nueva clave de memoria compartida por cliente, crea hilo 
    while (servidorActivo){
        printf("Esperando conexión de cliente...\n");
        sem_wait(&(*datosCompartidos).semCliente);

        if (servidorActivo){
            pthread_mutex_lock(&mutexContador);
            key_t nuevaClave = claveShm + contadorClientes;
            contadorClientes++;
            pthread_mutex_unlock(&mutexContador);

            int shmidCliente = shmget(nuevaClave, sizeof(MemoriaCompartida), IPC_CREAT | 0666);

            if (shmidCliente >= 0){
                MemoriaCompartida *memCliente = (MemoriaCompartida *)shmat(shmidCliente, NULL, 0);
                if (memCliente != (MemoriaCompartida *)-1){
                    // Copiar datos del cliente
                    memcpy(&(*memCliente).datos, &(*datosCompartidos).datos, sizeof(DatosCliente));
                    (*memCliente).claveCliente = nuevaClave;
                    (*datosCompartidos).claveCliente = nuevaClave;

                    printf("Cliente conectado - PID: %d\n", (*memCliente).datos.pidCliente);

                    pthread_t hilo;
                    if (pthread_create(&hilo, NULL, atencionCliente, (void *)memCliente) != 0){
                        perror("pthread_create");
                        shmdt(memCliente);
                        shmctl(shmidCliente, IPC_RMID, NULL);
                    }
                    else{
                        pthread_detach(hilo);
                    }
                }
            }
            sem_post(&(*datosCompartidos).semServidor);
        }
    }

    return 0;
}