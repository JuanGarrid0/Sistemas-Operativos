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

#include "restaurante.h"
#include <fcntl.h>
#include <sys/stat.h>
#define  MQ_NAME "/mq_cola_queue"

pid_t pid_sala, pid_cocina;
sem_t sem_preparado;
sem_t sem_cocinado;
sem_t sem_emplatado;
struct mq_attr attributes = {
    .mq_flags = 0,
    .mq_maxmsg = 15,
    .mq_msgsize = sizeof(Comanda),
    .mq_curmsgs = 0
};

 

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void* preparar_ingredientes(void* args) {
    char buffer[128];
    strcpy(buffer, (char*)args); 
    while (1) {
        sem_wait(&sem_preparado);
        printf("[Preparación] Esperando comanda...\n");
        printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n", buffer);
        sleep(tiempo_aleatorio(3, 6));
        printf("[Preparación] Ingredientes listos.\n");
        sem_post(&sem_cocinado);
    }
}

void* cocinar(void* arg) {

    while (1) {
        sem_wait(&sem_cocinado);
        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
     
        printf("[Cocina] Plato cocinado.\n");
        sem_post(&sem_emplatado);
    }
}

// Hilo para el emplatado
void* emplatar(void* arg) {

    while (1) {
        sem_wait(&sem_emplatado);
        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
   
        printf("[Emplatado] Plato listo y emplatado.\n");
        sem_post(&sem_preparado);

    }
}

int main(int argc, char* argv[]) {
    mqd_t queue = mq_open(MQ_NAME , O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &attributes);
    Comanda pedido;
    strcpy(pedido.msg,"Una de champiñones");
    mq_send(queue, (char *)&pedido, sizeof(pedido), 1);

    sem_init(&sem_preparado, 0, 1);
    sem_init(&sem_cocinado, 0, 1);
    sem_init(&sem_emplatado, 0, 0);
    pid_sala = fork();
    srand(time(NULL));

    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */
      


        } else {
            /* Proceso Cocina */
            pthread_t t1, t2, t3;
          
            printf("[Cocina] Comienzo de la preparación de platos...\n");
            if( pthread_create(&t1, NULL, &preparar_ingredientes, pedido.msg)  != 0) {
                return 1;
            };
            pthread_join(t1, NULL);


            if( pthread_create(&t2, NULL, &cocinar, NULL) != 0) {
                return 2;
            };
            pthread_join(t2, NULL);


            if( pthread_create(&t3, NULL, &emplatar, NULL) != 0) {
                return 3;
            };
            pthread_join(t3, NULL);


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