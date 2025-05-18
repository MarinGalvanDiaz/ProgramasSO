#include <stdio.h>         // Para entrada/salida estándar
#include <stdlib.h>        // Para funciones como exit()
#include <string.h>        // Para funciones de manejo de cadenas
#include <unistd.h>        // Para funciones como getpid()
#include <sys/ipc.h>       // Para claves IPC
#include <sys/shm.h>       // Para manejo de memoria compartida
#include <semaphore.h>     // Para uso de semáforos POSIX

// Estructura que contiene un mensaje y el PID del cliente
typedef struct{
    char mensaje[256];
    pid_t pidCliente;
} DatosCliente;

// Estructura de la memoria compartida con semáforos, datos y una clave
typedef struct{
    sem_t semServidor;       // Semáforo para que el servidor continúe
    sem_t semCliente;        // Semáforo para que el cliente continúe
    DatosCliente datos;      // Datos del cliente (mensaje y PID)
    key_t claveCliente;      // Clave específica para la memoria del cliente
} MemoriaCompartida;

// Función auxiliar para contar cuántas palabras hay en una cadena
int contarPalabras(const char *cadena){
    int palabras = 0;
    int enPalabra = 0;

    // Recorre cada carácter de la cadena
    while (*cadena){
        // Si encuentra un separador, marca que no está en una palabra
        if (*cadena == ' ' || *cadena == '\n' || *cadena == '\t'){
            enPalabra = 0;
        }
        // Si encuentra un carácter válido y no estaba dentro de una palabra, cuenta una nueva
        else if (!enPalabra){
            enPalabra = 1;
            palabras++;
        }
        cadena++;
    }

    return palabras;
}

int main(){
    // Asegura que exista un archivo base para generar la clave IPC con ftok
    FILE *f = fopen("memfile", "a");
    if (f == NULL){
        perror("fopen memfile");
        exit(1);
    }
    fclose(f);

    // Genera clave única a partir del archivo y un identificador de proyecto
    key_t claveShm = ftok("memfile", 'M');
    if (claveShm == -1){
        perror("ftok");
        exit(1);
    }

    // Obtiene el ID del segmento de memoria compartida principal (global)
    int shmidGlobal = shmget(claveShm, sizeof(MemoriaCompartida), 0666);
    if (shmidGlobal < 0){
        perror("shmget");
        exit(1);
    }

    // Se enlaza a la memoria compartida
    MemoriaCompartida *datosCompartidos = (MemoriaCompartida *)shmat(shmidGlobal, NULL, 0);
    if (datosCompartidos == (MemoriaCompartida *)-1){
        perror("shmat");
        exit(1);
    }

    printf("----- CLIENTE (PID: %d) -----\n", getpid());

    // Validación para que el usuario ingrese exactamente 10 palabras
    int numPalabras;
    do{
        printf("Ingrese una frase de 10 palabras: ");

        // Lee la frase del usuario
        fgets((*datosCompartidos).datos.mensaje, sizeof((*datosCompartidos).datos.mensaje), stdin);

        // Elimina el salto de línea final que incluye fgets
        (*datosCompartidos).datos.mensaje[strcspn((*datosCompartidos).datos.mensaje, "\n")] = '\0';

        // Cuenta cuántas palabras hay
        numPalabras = contarPalabras((*datosCompartidos).datos.mensaje);

        // Informa si el número no es válido
        if (numPalabras != 10){
            printf("Error: Debe ingresar exactamente 10 palabras (ingresó %d)\n", numPalabras);
        }
    } while (numPalabras != 10);  // Repite hasta que haya 10 palabras

    // Guarda el PID del cliente en la estructura compartida
    (*datosCompartidos).datos.pidCliente = getpid();

    printf("Enviando mensaje al servidor...\n");

    // Señaliza al servidor que ya puede leer el mensaje
    sem_post(&(*datosCompartidos).semCliente);

    // Espera la respuesta del servidor
    sem_wait(&(*datosCompartidos).semServidor);

    // Obtiene la clave específica de memoria asignada al cliente
    key_t claveCliente = (*datosCompartidos).claveCliente;

    // Se conecta a la memoria específica que el servidor creó para este cliente
    int shmidCliente = shmget(claveCliente, sizeof(MemoriaCompartida), 0666);
    if (shmidCliente < 0){
        perror("shmget cliente");
        shmdt(datosCompartidos);  // Desvincula la memoria global antes de salir
        exit(1);
    }

    // Se enlaza a la memoria dedicada del cliente
    MemoriaCompartida *memCliente = (MemoriaCompartida *)shmat(shmidCliente, NULL, 0);
    if (memCliente == (MemoriaCompartida *)-1){
        perror("shmat cliente");
        shmdt(datosCompartidos);  // Desvincula la memoria global antes de salir
        exit(1);
    }

    // Muestra la respuesta procesada del servidor (puede incluir modificaciones al mensaje)
    printf("Respuesta del servidor recibida\n");
    printf("Mensaje procesado: %s\n", (*memCliente).datos.mensaje);

    // Libera ambas memorias compartidas
    shmdt(memCliente);
    shmdt(datosCompartidos);

    return 0;
}
