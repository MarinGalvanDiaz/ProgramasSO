#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>

//Función ejecutada por cada hilo
void *multiplicacion(void *fila) {
    printf("\nSoy el hilo %lu y voy a procesar los siguientes datos: ",pthread_self());

    int i = 0;
    int *datos = (int*)fila;
    
    //Impresión de valores a procesar
    for (i = 0; i < 9; i++)
    {
        printf("%d ",datos[i]);
    }
    printf("\n");

    int *resultado = malloc(sizeof(int));
    *resultado = 1;

    //Multiplicación de la fila
    for(i = 0; i < 9; i++) {
        *resultado *= datos[i];
    }

    printf("\nHilo %lu terminado. Multiplicación: %d\n",pthread_self(),*resultado);

    pthread_exit((void*)resultado);
}


int main(void) {
    int datos[3][9] = {
        {1, 2, 3, 4, 5, 6, 7, 8, 9},
        {1, 3, 5, 7, 9, 11, 13, 15, 17},
        {2, 4, 6, 8, 10, 12, 14, 16, 18}
    };

    pthread_t hilos[3];
    int i = 0;
    int *resultados[3];

    //Creación de los hilos 
    for(i = 0; i < 3; i++) {
        pthread_create(&hilos[i],NULL,multiplicacion,(void*)datos[i]);
    }

    //El proceso padre hilo espera a que los hilos hijos finalicen 
    for(i = 0; i < 3; i++) {
        pthread_join(hilos[i],(void**)&resultados[i]);
    }

    //Impresión de resultados finales
    printf("\nResultados finales:\n");
    for (i = 0; i < 3; i++) {
        printf("Resultados del hilo %lu: %d\n",hilos[i],*resultados[i]);
        free(resultados[i]);
    }
    
    return 0;
}