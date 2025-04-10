#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>

#define PERMISOS 0644

int crear_semaforo(key_t llave, int valor_inicial) {
    int semid = semget(llave, 1, IPC_CREAT | PERMISOS);
    if (semid == -1) {
        perror("Error creando semaforo");
        exit(EXIT_FAILURE);
    }
    semctl(semid, 0, SETVAL, valor_inicial);
    return semid;
}

void down(int semid) {
    struct sembuf op_p = {0, -1, 0};
    semop(semid, &op_p, 1);
}

void up(int semid) {
    struct sembuf op_v = {0, +1, 0};
    semop(semid, &op_v, 1);
}

int main() {
    int dato[3][9] = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9},
        {1, 3, 5, 7, 9, 11, 15, 17, 19},
        {2, 4, 6, 8, 10, 12, 14, 16, 18}
    };

    
    key_t llave_sem1 = ftok("Archivo1", 'k');
    key_t llave_sem2 = ftok("Archivo2", 'k');
    key_t llave_sem3 = ftok("Archivo3", 'k');
    key_t llave_sem_mem = ftok("ArchivoMem", 's'); 

    int sem1 = crear_semaforo(llave_sem1, 0);
    int sem2 = crear_semaforo(llave_sem2, 0);
    int sem3 = crear_semaforo(llave_sem3, 0);
    int sem_mem = crear_semaforo(llave_sem_mem, 1); 

    
    key_t llave_memoria = ftok("Archivo_memoria", 'm');
    int shm_id = shmget(llave_memoria, (3 * sizeof(int)) + sizeof(dato), IPC_CREAT | PERMISOS);
    int *shared_mem = (int *)shmat(shm_id, 0, 0);

    int *resultados = (int *)shared_mem;
    int (*mem_dato)[9] = (int (*)[9])(shared_mem + 3 * sizeof(int));

   
    down(sem_mem);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 9; j++)
            mem_dato[i][j] = dato[i][j];
    up(sem_mem);

    key_t llave_sem_resultados = ftok("ArchivoResultados", 'r');
int sem_resultados = crear_semaforo(llave_sem_resultados, 1); 

pid_t pid1 = fork();
if (pid1 < 0) {
    perror("Error en fork hijo 1");
    exit(EXIT_FAILURE);
}
if (pid1 == 0) {
    printf("Hijo 1 iniciado.\n");
    int suma = 0;
    down(sem_mem);
    for (int i = 0; i < 9; i++) suma += mem_dato[0][i];
    up(sem_mem);

    down(sem_resultados); 
    resultados[0] = suma;
    up(sem_resultados);

    printf("Hijo 1 terminado. Suma: %d\n", suma);
    exit(0);
}

pid_t pid2 = fork();
if (pid2 < 0) {
    perror("Error en fork hijo 2");
    exit(EXIT_FAILURE);
}
if (pid2 == 0) {
    printf("Hijo 2 iniciado.\n");
    int suma = 0;
    down(sem_mem);
    for (int i = 0; i < 9; i++) suma += mem_dato[1][i];
    up(sem_mem);

    down(sem_resultados); 
    resultados[1] = suma;
    up(sem_resultados);

    printf("Hijo 2 terminado. Suma: %d\n", suma);
    exit(0);
}

pid_t pid3 = fork();
if (pid3 < 0) {
    perror("Error en fork hijo 3");
    exit(EXIT_FAILURE);
}
if (pid3 == 0) {
    printf("Hijo 3 iniciado.\n");
    int suma = 0;
    down(sem_mem);
    for (int i = 0; i < 9; i++) suma += mem_dato[2][i];
    up(sem_mem);

    down(sem_resultados); 
    resultados[2] = suma;
    up(sem_resultados);

    printf("Hijo 3 terminado. Suma: %d\n", suma);
    exit(0);
}

printf("Padre esperando resultados...\n");

waitpid(pid1, NULL, 0);
waitpid(pid2, NULL, 0);
waitpid(pid3, NULL, 0);

down(sem_resultados);
printf("Resultados finales:\n");
for (int i = 0; i < 3; i++) {
    printf("Resultado del hijo %d: %d\n", i + 1, resultados[i]);
}
up(sem_resultados);

shmdt(shared_mem);
shmctl(shm_id, IPC_RMID, NULL);
semctl(sem1, 0, IPC_RMID);
semctl(sem2, 0, IPC_RMID);
semctl(sem3, 0, IPC_RMID);
semctl(sem_mem, 0, IPC_RMID);
semctl(sem_resultados, 0, IPC_RMID);

return 0;
}