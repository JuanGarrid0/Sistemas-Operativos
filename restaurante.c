#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <mqueue.h>
#include <string.h>

pid_t pid_sala, pid_cocina;

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void* preparar_ingredientes(void* args) {
    char buffer[128];
    while (1) {

        printf("[Preparación] Esperando comanda...\n");

        printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n", buffer);
        sleep(tiempo_aleatorio(3, 6));
        printf("[Preparación] Ingredientes listos.\n");

    }
}

void* cocinar(void* arg) {

    while (1) {

        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
        printf("[Cocina] Plato cocinado.\n");

    }
}

// Hilo para el emplatado
void* emplatar(void* arg) {

    while (1) {

        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
        printf("[Emplatado] Plato listo y emplatado.\n");

    }
}

int main(int argc, char* argv[]) {

    pid_sala = fork();

    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */



        } else {
            /* Proceso Cocina */
            printf("[Cocina] Comienzo de la preparación de platos...\n");



        }
    } else {
        /* Proceso Sala */
        printf("[Sala] Inicio de la gestión de comandas...\n");

	int num_comanda = 0;

        while (1) {

            sleep(tiempo_aleatorio(5, 10));
            printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);


            num_comanda++;

        }
    }

    exit(0);
}