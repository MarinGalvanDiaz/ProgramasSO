#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

//Mensaje y PID del cliente
typedef struct DatosCliente {
    char mensaje[256];
    pid_t pidCliente;
} DatosCliente;

//Estructura de la memoria compartida con semáforos, datos y una clave
typedef struct MemoriaCompartida {
    sem_t semServidor;
    sem_t semCliente;
    DatosCliente datos;
    key_t claveCliente;
} MemoriaCompartida;

int memoria_global; 
MemoriaCompartida *datosCompartidos;
volatile sig_atomic_t servidorActivo = 1;
int contadorClientes = 1; //Para que cada cliente tenga una clave única 
pthread_mutex_t mutexContador = PTHREAD_MUTEX_INITIALIZER; //Evita condiciones de carrera en el contador
pthread_mutex_t mutexArchivo = PTHREAD_MUTEX_INITIALIZER; //Evita condiciones de carrera en la escritura/lectura del archivo
pthread_cond_t condArchivoListo = PTHREAD_COND_INITIALIZER; //Indica al hilo lector cuando se ha realizado un cambio en el archivo
int archivoListo = 0;  //Bandera compartida para el archivo


//Libera recursos correctamente al finalizar
void liberarRecursos() {
    printf("\nLiberando recursos...\n");
    sem_destroy(&(datosCompartidos->semServidor));
    sem_destroy(&(datosCompartidos->semCliente));
    shmdt(datosCompartidos);
    shmctl(memoria_global, IPC_RMID, NULL);
    pthread_mutex_destroy(&mutexContador);
    pthread_mutex_destroy(&mutexArchivo);
    pthread_cond_destroy(&condArchivoListo);
    remove("frases.txt");
    printf("Recursos liberados. Saliendo del servidor\n");
    exit(0);
}

//Al darse la señal ctrl + c, cambia servidor activo
void manejarSenial(int sig) {
    if (sig == SIGINT)
    {
        printf("\nSeñal de terminación recibida. Apagando servidor...\n");
        servidorActivo = 0;
        sem_post(&datosCompartidos->semCliente);
        pthread_cond_signal(&condArchivoListo);
    }
}

//Función para el hilo del cliente
void *atencionCliente(void *arg) {
    MemoriaCompartida *memHilo = (MemoriaCompartida *)arg;
    printf("\n--- Hilo atendiendo cliente PID: %d ---\n", memHilo->datos.pidCliente);

    pthread_mutex_lock(&mutexArchivo);

    // Escribe el mensaje
    FILE *frases = fopen("frases.txt", "a");
    if (frases) {
        fprintf(frases, "%s\n", memHilo->datos.mensaje);
        fclose(frases);
        archivoListo = 1;  //Señala que el archivo ya está listo
        pthread_cond_signal(&condArchivoListo);  //Despierta al lector
    } else {
        perror("fopen frases.txt");
    }

    pthread_mutex_unlock(&mutexArchivo);


    int shmid = shmget(memHilo->claveCliente, 0, 0);
    shmdt(memHilo);
    if (shmid >= 0){
        shmctl(shmid, IPC_RMID, NULL);
    }
        
    pthread_exit(NULL);
}

//Función para el hilo que lee frases del archivo "frases.txt"
void *leerFrase(void *arg) {
    while (servidorActivo) {
        pthread_mutex_lock(&mutexArchivo);

        //Espera hasta que el escritor indique que hay una nueva frase
        while (!archivoListo && servidorActivo) {
            pthread_cond_wait(&condArchivoListo, &mutexArchivo);
        }

        if (!servidorActivo) {
            pthread_mutex_unlock(&mutexArchivo);
            break;
        }

        //Resetear bandera para próximas escrituras
        archivoListo = 0;

        //Leer archivo
        FILE *frases = fopen("frases.txt", "r");
        if (frases) {
            char linea[256], ultima[256] = "";
            while (fgets(linea, sizeof(linea), frases)) {
                strncpy(ultima, linea, sizeof(ultima)-1);
                ultima[sizeof(ultima)-1] = '\0';
            }
            fclose(frases);
            printf("Frase leída en el archivo: %s", ultima);
        } else {
            perror("fopen frases.txt");
        }
        pthread_mutex_unlock(&mutexArchivo);
    }
    pthread_exit(NULL);
}

int main(void) {
    //Maneja la señal de terminación del servidor
    signal(SIGINT, manejarSenial);
    atexit(liberarRecursos);

    //Genera clave para la memoria compartida global
    FILE *f = fopen("memoriaPrincipal", "w");
    if (f == NULL){
        perror("fopen memoriaPrincipal");
        exit(1);
    }
    fclose(f);

    key_t clave_memoria = ftok("memoriaPrincipal", 'P');
    if (clave_memoria == -1){
        perror("ftok memoriaPrincipal");
        exit(1);
    }

    //Creación de la memoria compartida global
    memoria_global = shmget(clave_memoria, sizeof(MemoriaCompartida), IPC_CREAT | 0666);
    if (memoria_global < 0){
        perror("shmget memoria_global");
        exit(1);
    }
    datosCompartidos = (MemoriaCompartida *)shmat(memoria_global, NULL, 0);
    if (datosCompartidos == (MemoriaCompartida *)-1){
        perror("shmat datosCompartidos");
        exit(1);
    }

    //Creación de semáforos
    sem_init(&datosCompartidos->semServidor, 1, 0);
    sem_init(&datosCompartidos->semCliente, 1, 0);

    //Creación del hilo para leer la última frase añadida
    pthread_t hiloLectura;
    pthread_create(&hiloLectura, NULL, leerFrase, NULL);
    pthread_detach(hiloLectura);

    printf("----- SERVIDOR INICIADO -----\n");
    printf("Presione Ctrl + C para terminar el servidor\n\n");

    /*  Espera a un cliente, crea un segmento de memoria y un hilo para cada cliente,
        después guarda el mensaje del cliente en el archivo "frases.txt" y luego el 
        servidor detectara el cambio en el archivo y leerá la ultima frase con su 
        hilo lector previamente inicializado */

    while (servidorActivo)
    {
        printf("Esperando conexión de cliente...\n");
        sem_wait(&datosCompartidos->semCliente);

        if (!servidorActivo) break;

        //Generar clave única de forma segura
        pthread_mutex_lock(&mutexContador);
        key_t nuevaClave = clave_memoria + contadorClientes++;
        pthread_mutex_unlock(&mutexContador);

        //Crear memoria compartida para el cliente
        int shmidCliente = shmget(nuevaClave, sizeof(MemoriaCompartida), IPC_CREAT | 0666);
        if (shmidCliente < 0) {
            perror("shmget");
            continue;
        }

        //Adjuntar memoria
        MemoriaCompartida *memCliente = (MemoriaCompartida *)shmat(shmidCliente, NULL, 0);
        if (memCliente == (MemoriaCompartida *)-1) {
            perror("shmat");
            shmctl(shmidCliente, IPC_RMID, NULL);  //Limpiar si shmat falla
            continue;
        }

        //Copia datos del cliente
        memcpy(&memCliente->datos, &datosCompartidos->datos, sizeof(DatosCliente));
        memCliente->claveCliente = nuevaClave;
        datosCompartidos->claveCliente = nuevaClave;

        printf("Cliente conectado - PID: %d\n", memCliente->datos.pidCliente);

        //Creación del hilo para el cliente
        pthread_t hiloCliente;
        if (pthread_create(&hiloCliente, NULL, atencionCliente, (void *)memCliente) != 0){
            perror("pthread_create");
            shmdt(memCliente);
            shmctl(shmidCliente, IPC_RMID, NULL);
        }
        else{
            pthread_detach(hiloCliente);
        }

        sem_post(&datosCompartidos->semServidor);
    }

    return 0;
}