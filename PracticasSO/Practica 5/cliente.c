#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

//Mensaje y PID del cliente
typedef struct{
    char mensaje[256];
    pid_t pidCliente;
} DatosCliente;

//Estructura de la memoria compartida con semáforos, datos y una clave
typedef struct{
    sem_t semServidor;
    sem_t semCliente;
    DatosCliente datos;
    key_t claveCliente;
} MemoriaCompartida;

//Función auxiliar para contar cuántas palabras hay en la cadena
int contarPalabras(const char *cadena) {
    int palabras = 0;
    int enPalabra = 0;

    while (*cadena) {
        if (*cadena == ' ' || *cadena == '\n' || *cadena == '\t') {
            enPalabra = 0;
        }
        else if (!enPalabra) {
            enPalabra = 1;
            palabras++;
        }
        cadena++;
    }

    return palabras;
}

int main(void) {
    //Asegura que exista un archivo base para generar la clave IPC con ftok
    FILE *f = fopen("memoriaPrincipal", "r");
    if (f == NULL){
        perror("fopen memoriaPrincipal");
        exit(1);
    }
    fclose(f);

    //Genera clave única a partir del archivo y un identificador de proyecto
    key_t clave_memoria = ftok("memoriaPrincipal", 'P');
    if (clave_memoria == -1){
        perror("ftok memoriaPrincipal");
        exit(1);
    }

    //Obtiene el ID del segmento de memoria compartida principal
    int shmidGlobal = shmget(clave_memoria, sizeof(MemoriaCompartida), 0666);
    if (shmidGlobal < 0){
        perror("shmget shmidGlobal");
        exit(1);
    }

    //Vinculación a la memoria compartida
    MemoriaCompartida *datosCompartidos = (MemoriaCompartida *)shmat(shmidGlobal, NULL, 0);
    if (datosCompartidos == (MemoriaCompartida *)-1){
        perror("shmat datosCompartidos");
        exit(1);
    }

    printf("----- CLIENTE (PID: %d) -----\n", getpid());

    //Validación para que el usuario ingrese exactamente 10 palabras
    int numPalabras;
    do{
        printf("Ingrese una frase de 10 palabras: ");

        //Lee la frase del usuario
        fgets((*datosCompartidos).datos.mensaje, sizeof((*datosCompartidos).datos.mensaje), stdin);

        //Elimina el salto de línea final que incluye fgets
        (*datosCompartidos).datos.mensaje[strcspn((*datosCompartidos).datos.mensaje, "\n")] = '\0';

        //Cuenta cuántas palabras hay
        numPalabras = contarPalabras((*datosCompartidos).datos.mensaje);

        //Informa si el número no es válido
        if (numPalabras != 10){
            printf("Error: Debe ingresar exactamente 10 palabras (ingresó %d)\n", numPalabras);
        }
    } while (numPalabras != 10);

    //Guarda el PID del cliente en la estructura compartida
    (*datosCompartidos).datos.pidCliente = getpid();

    printf("Enviando mensaje al servidor...\n");

    //Señaliza al servidor que ya puede leer el mensaje
    sem_post(&(*datosCompartidos).semCliente);

    //Espera la respuesta del servidor
    sem_wait(&(*datosCompartidos).semServidor);

    //Obtiene la clave específica de memoria asignada al cliente
    key_t claveCliente = (*datosCompartidos).claveCliente;

    //Se conecta a la memoria específica que el servidor creó para este cliente
    int shmidCliente = shmget(claveCliente, sizeof(MemoriaCompartida), 0666);
    if (shmidCliente < 0){
        perror("shmget cliente");
        shmdt(datosCompartidos);  //Desvincula la memoria global antes de salir
        exit(1);
    }

    //Se enlaza a la memoria dedicada del cliente
    MemoriaCompartida *memCliente = (MemoriaCompartida *)shmat(shmidCliente, NULL, 0);
    if (memCliente == (MemoriaCompartida *)-1){
        perror("shmat cliente");
        shmdt(datosCompartidos);  //Desvincula la memoria global antes de salir
        exit(1);
    }

    //Muestra la respuesta procesada del servidor
    printf("Respuesta del servidor recibida\n");
    printf("Mensaje procesado: %s\n", (*memCliente).datos.mensaje);

    //Libera ambas memorias compartidas
    shmdt(memCliente);
    shmdt(datosCompartidos);

    return 0;
}
