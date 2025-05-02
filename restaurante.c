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
#define  MQ_NAME "/mY_queue"
#define BUFF_SIZE 8192

pid_t pid_sala, pid_cocina;
sem_t sem_preparado, sem_cocinado, sem_emplatado;
mqd_t queue;
Comanda pedido;
pthread_t t1, t2, t3;
struct mq_attr attributes = {
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(Comanda),
    .mq_curmsgs = 0
};

char buffer_pedido[BUFF_SIZE];
 

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


    sem_init(&sem_preparado, 0, 1);
    sem_init(&sem_cocinado, 0, 0);
    sem_init(&sem_emplatado, 0, 0);
    pid_sala = fork();
    srand(time(NULL));

    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */
            waitpid(pid_cocina, NULL,0);
            waitpid(pid_sala,NULL, 0);




        } else {
            /* Proceso Cocina */
            queue = mq_open(MQ_NAME ,O_RDONLY);
            mq_receive (queue, buffer_pedido, attributes.mq_msgsize, 0);
            printf("El pedido es %s \n",buffer_pedido);
            

            printf("[Cocina] Comienzo de la preparación de platos...\n");
            if( pthread_create(&t1, NULL, &preparar_ingredientes, buffer_pedido)  != 0) {
                return 1;
            };


            if( pthread_create(&t2, NULL, &cocinar, NULL) != 0) {
                return 2;
            };


            if( pthread_create(&t3, NULL, &emplatar, NULL) != 0) {
                return 3;
            };

            mq_close (queue);

            pthread_join(t1, NULL);
            pthread_join(t2, NULL);
            pthread_join(t3, NULL);
            free (buffer_pedido);
            mq_close (queue);
            mq_unlink(MQ_NAME);
        }
    } else {
        /* Proceso Sala */
        queue = mq_open(MQ_NAME , O_CREAT | O_WRONLY, 0664 , &attributes);
        printf("[Sala] Inicio de la gestión de comandas...\n");
        if (queue == -1){
            perror ("mq_open");
            exit (1);
        }
        strcpy(pedido.msg,"Una de champiñones");
        mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
        mq_close(queue);

	int num_comanda = 0;
        mq_getattr (queue, &attributes);                                //borrable
        printf ("%ld pedidos en cocina.\n", attributes.mq_curmsgs);      //borrable, saca numero de mensajes en cola
        while (1) {

            sleep(tiempo_aleatorio(5, 10));
            printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);


            num_comanda++;
        }   
    }

    exit(0);
}