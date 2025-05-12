#define _POSIX_C_SOURCE 200809L

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
#define  MQ_NAME "/my_queue"
#define BUFF_SIZE 8192
 
pid_t pid_sala, pid_cocina;                             //process ids
sem_t sem_preparado, sem_cocinado, sem_emplatado;       //semaforos
mqd_t queue;                                            //cola de POSIX
Comanda pedido;                                         
int ready=0;
pthread_t t1, t2, t3;                                   //hilos
struct mq_attr attributes = {       
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(Comanda),
    .mq_curmsgs = 0
};                                                      //atributos de la cola
char *buffer_pedido;                                    //buffer para los pedidos

void plato_listo(int sig){
        ready = 1;
 }

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

//Hilo para preparado
void* preparar_ingredientes(void* args) {
    char buffer[128];
    strcpy(buffer, (char*)args); //se copia el parametro de la funcion en el buffer
    while (1) {
        sem_wait(&sem_preparado);                                                                       //Rojo
        printf("[Preparación] Esperando comanda...\n");
        printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n", buffer);
        sleep(tiempo_aleatorio(3, 6));
        printf("[Preparación] Ingredientes listos.\n");

        sem_post(&sem_cocinado);                                                                        //Verde
 
    }
}

//Hilo para cocinar
void* cocinar(void* arg) {

    while (1) {
        sem_wait(&sem_cocinado);                                                                        //Rojo
        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
     
        printf("[Cocina] Plato cocinado.\n");
        sem_post(&sem_emplatado);                                                                       //Verde
    }
}

// Hilo para el emplatado
void* emplatar(void* arg) {

    while (1) {
        sem_wait(&sem_emplatado);                                                                       //Rojo
        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
   
        printf("[Emplatado] Plato listo y emplatado.\n");
         
       
        sem_post(&sem_preparado);                                                                       //Verde
        kill(pid_sala, SIGUSR1);


    }

}

void handle(int sig){
    //Esperamos acabar
    waitpid(pid_cocina, NULL,0);
    waitpid(pid_sala,NULL, 0);
    //fin hilos
    pthread_join(t3, NULL);
    pthread_join(t2, NULL);
    pthread_join(t1, NULL);

    //cerramos cola y borramos del sistema
    mq_close (queue);
    mq_unlink(MQ_NAME); 

    //destruimos semaforos
    sem_destroy(&sem_cocinado);
    sem_destroy(&sem_emplatado);
    sem_destroy(&sem_preparado);

    //Restauramos la señal
    signal(SIGINT, SIG_DFL);


    exit(0);

   
}

int main(int argc, char* argv[]) {
    //memory allocation del buffer
    buffer_pedido = malloc(1024);
    //Inicializamos semaforos
    sem_init(&sem_preparado, 0, 1);
    sem_init(&sem_cocinado, 0, 0);
    sem_init(&sem_emplatado, 0, 0);
 
    pid_sala = fork();
    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */
            //Usamos sigaction para gestionar la señal SIGINT con la funcion handle
            signal(SIGINT, handle);
            /*antes de cerrar el programa, se espera a que acaben los procesos hijos, en vez de
             usar los id de los procesos, -1 hace esperar a todos los procesos hijos*/
            waitpid(-1, NULL,0);
        } else {
            /* Proceso Cocina */
            //Abrimos la cola en modo lectura y guardamos en el buffer
            queue = mq_open(MQ_NAME ,O_RDONLY);
            mq_receive (queue, buffer_pedido, attributes.mq_msgsize, 0);
            //Comprobamos si hay algun fallo que haga que el dato de la cola no llegue al buffer
            printf("El pedido es %s \n",buffer_pedido);
            

            printf("[Cocina] Comienzo de la preparación de platos...\n");
            /*Arrancamos los hilos de cada uno de los procesos uno a uno, y esperamos a que acabe el ultimo para cerrar los hilos,
             de esta forma los pedidos se hacen de uno en uno. Ejemplo: Se prepara, cocina y emplata A, se prepara, cocina y emplata B */
            if( pthread_create(&t1, NULL, &preparar_ingredientes, buffer_pedido)  != 0) {
                return 1;
            };


            if( pthread_create(&t2, NULL, &cocinar, NULL) != 0) {
                return 2;
            };


            if( pthread_create(&t3, NULL, &emplatar, NULL) != 0) {
                return 3;
            };


            pthread_join(t3, NULL);
            pthread_join(t2, NULL);
            pthread_join(t1, NULL);

            //Al final de cada comanda liberamos el buffer y cerramos cola--> Cerrar y liberar mas arriba si se quiere hacer mas de un pedido cada vez
            free(buffer_pedido);
            mq_close (queue);

        }
    } else {
        /* Proceso Sala */
        //Esperamos plato
        struct sigaction sa_emplatado = {0};
        sa_emplatado.sa_handler = plato_listo;
        sigaction(SIGUSR1, &sa_emplatado, NULL);
        //Creamos la cola POSIX
        queue = mq_open(MQ_NAME , O_CREAT | O_WRONLY, 0664 , &attributes);
        printf("[Sala] Inicio de la gestión de comandas...\n");
        if (queue == -1){
            perror ("mq_open");
            exit (1);
        }
        // copiamos el pedido en una variable tipo Comanda, que es aceptada por la cola y enviamos dicha Comanda, despues cerramos la cola
        strcpy(pedido.msg,"Una de champiñones");
        mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
        mq_close(queue);
 
	    int num_comanda = 0;
        //Para comprobar el numero de pedidos en cola en t0, inicia en 0, luego llega la comanda
        mq_getattr (queue, &attributes);                                //borrable
        printf ("%ld pedidos en cocina.\n", attributes.mq_curmsgs);      //borrable, saca numero de mensajes en cola
        while (1) {
            while (!ready) pause(); 
                sleep(tiempo_aleatorio(5, 10));
                printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);
                num_comanda++;
                ready = 0;
         }   
    }  

    exit(0);
}