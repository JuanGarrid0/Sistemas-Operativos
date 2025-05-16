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
#include <unistd.h>

#include <termios.h>
#include "restaurante.h"
#include <fcntl.h>
#include <sys/stat.h>
#define  MQ_NAME "/my_queue"
#define BUFF_SIZE 8192
 
pid_t pid_sala, pid_cocina;                             //process ids
sem_t *sem_preparado;
sem_t *sem_cocinado;
sem_t *sem_emplatado;
mqd_t queue;                                            //cola de POSIX
Comanda pedido;                                         
pthread_t t1, t2, t3;                                   //hilos
struct mq_attr attributes = {       
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(Comanda),
    .mq_curmsgs = 0
};                                                      //atributos de la cola
char c;                                                 // entrada desde consola ("P")
int ascii;                                                //conversion de char a entero
pthread_t t_sala_input;                                     //Hilo de sala
Comanda recibido;
char* buffer_pedido;

//Flags globales para finalizar procesos
volatile sig_atomic_t finalizar = 0;
volatile sig_atomic_t ready = 0;
//Señal que protege la flag global de finalizado
void sigint_handler(int sig) {
    finalizar = 1;
}

void plato_listo(int sig){
        ready = 1;
 }

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

//Hilo para preparado
void* preparar_ingredientes(void* args) {

    printf("[Preparación] Hilo iniciado.\n");
    queue = mq_open(MQ_NAME, O_RDONLY);
    if (queue == -1) {
        perror("[Cocina] Error al abrir la cola");
        exit(EXIT_FAILURE);
    }            

    while (!finalizar /*&& !ready*/) {
        sem_wait(sem_preparado);
        if (mq_receive(queue, (char*)&recibido.msg, attributes.mq_msgsize, 0) == -1) {
            perror("[Cocina] Error al recibir pedido");
        } else {
            //Comprobamos si hay algun fallo que haga que el dato de la cola no llegue al buffer
            printf("[Cocina] Pedido recibido: %s\n", recibido.msg);
        }

        printf("[Preparación] Preparando ingredientes...\n");
        sleep(tiempo_aleatorio(3, 6));
        printf("[Preparación] Ingredientes listos.\n");

        sem_post(sem_cocinado);
    }
    mq_close(queue);
    printf("[Preparación] Hilo finalizando.\n");
    return NULL;
}


//Hilo para cocinar
void* cocinar(void* arg) {
    while (!finalizar && !ready) {
        sem_wait(sem_cocinado);
        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
        printf("[Cocina] Plato cocinado.\n");
        sem_post(sem_emplatado);
    }
    printf("[Cocina] Hilo finalizando.\n");
    return NULL;
}


// Hilo para el emplatado
void* emplatar(void* arg) {

    while (!finalizar && !ready) {
        sem_wait(sem_emplatado);   

        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
        printf("[Emplatado] Plato listo y emplatado.\n");
        kill(pid_sala, SIGUSR1);                                //Manda ready = 1 --> plato listo
    }
    printf("[Emplatado] Hilo finalizando.\n");
    return NULL;
}
 

void* escuchar_teclado(void* arg) {
    while (!finalizar) {
        sem_preparado = sem_open("/sem_preparado", O_CREAT, 0666, 0);
        scanf("%c", &c);                //input de terminal
        ascii = c;                      // convierte a int
        if (ascii == 112) {  // valor de "p", esta testeado, es 112
            queue = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0664, &attributes);
            mq_getattr(queue, &attributes);
            if (attributes.mq_curmsgs >= attributes.mq_maxmsg) {
                printf("[Sala] La sala está saturada, intentelo de nuevo más tarde.\n");
            } else {

                scanf("%s", buffer_pedido);
                //char* prueba ="torreznos";
                strcpy(pedido.msg, buffer_pedido);
                printf("El pedido que se ha hecho es de %s\n",pedido.msg);
                mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
                sem_post(sem_preparado);
                printf("[Sala] Pedido añadido desde teclado.\n");
                mq_close(queue);
                ready = 0;
                fflush(stdout);
            }
        }
    }
    return NULL;
}




int main(int argc, char* argv[]) {
        

 
    pid_sala = fork();
    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */
            
            
            // Configuramos sigaction para capturar SIGINT (Ctrl+C) y establecer el handler personalizado sigint_handler.
            // Esto asegura que el programa pueda realizar una terminación ordenada y controlada, propagando la señal a los procesos e hilos.
            struct sigaction sa = {0};
            sa.sa_handler = sigint_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;  // Importante: no usar SA_RESTART para que pause() devuelva EINTR
            sigaction(SIGINT, &sa, NULL);
            
            printf("[Padre] Esperando señal SIGINT...\n");
            
            // Esperamos señal SIGINT de manera bloqueante
            while (!finalizar) pause();
            // Una vez capturado SIGINT, enviamos la señal manualmente a los procesos hijos para que terminen sus bucles de trabajo
            kill(pid_cocina, SIGINT);
            kill(pid_sala, SIGINT);

            // Esperamos explícitamente a la terminación de cada proceso hijo
            waitpid(pid_cocina, NULL, 0);
            waitpid(pid_sala, NULL, 0);

            printf("[Padre] Procesos hijos finalizados. Saliendo del programa.\n");
            exit(EXIT_SUCCESS);


        } else {
            /* Proceso Cocina */
            sem_preparado = sem_open("/sem_preparado", O_CREAT, 0666, 1);
            sem_cocinado = sem_open("/sem_cocinado", O_CREAT, 0666, 0);
            sem_emplatado = sem_open("/sem_emplatado", O_CREAT, 0666, 0);
            struct sigaction sa_cocina = {0};
            sa_cocina.sa_handler = sigint_handler;
            sigemptyset(&sa_cocina.sa_mask);
            sa_cocina.sa_flags = 0;
            sigaction(SIGINT, &sa_cocina, NULL);

            //Abrimos la cola en modo lectura y guardamos en el buffer
            // Abrimos la cola en modo lectura y guardamos en el buffer
           
            printf("[Cocina] Comienzo de la preparación de platos...\n");

            /*Arrancamos los hilos de cada uno de los procesos uno a uno, y esperamos a que acabe el ultimo para cerrar los hilos,
            de esta forma los pedidos se hacen de uno en uno. Ejemplo: Se prepara, cocina y emplata A, se prepara, cocina y emplata B */
            if (pthread_create(&t1, NULL, &preparar_ingredientes, NULL) != 0) {
                perror("[Cocina] Error al crear hilo Preparar");
                exit(EXIT_FAILURE);
            }
            
            if (pthread_create(&t2, NULL, &cocinar, NULL) != 0) {
                perror("[Cocina] Error al crear hilo Cocinar");
                exit(EXIT_FAILURE);
            }
            
            if (pthread_create(&t3, NULL, &emplatar, NULL) != 0) {
                perror("[Cocina] Error al crear hilo Emplatar");
                exit(EXIT_FAILURE);
            }  
            while(!finalizar && !ready){
                    pause();
            }

            // Una vez recibido SIGINT, aseguramos que desbloqueamos los hilos si estuvieran esperando semáforos


            //ARREGLAR extra loop
            //sleep(5);
            //sem_post(sem_preparado);  
            //sem_post(sem_cocinado);   
            //sem_post(sem_emplatado);   
            //printf("Se va por cocina  I");

            // Esperamos a que los hilos terminen ordenadamente
            //pthread_join(t1, NULL);
            //pthread_join(t2, NULL);
            //pthread_join(t3, NULL);
            
            //printf("Se va por cocina  II");

            //Al final de cada comanda liberamos el buffer y cerramos cola--> Cerrar y liberar mas arriba si se quiere hacer mas de un pedido cada vez
            mq_close (queue);
            mq_unlink(MQ_NAME);

            sem_close(sem_preparado);
            sem_close(sem_cocinado);
            sem_close(sem_emplatado);

            sem_unlink("/sem_preparado");
            sem_unlink("/sem_cocinado");
            sem_unlink("/sem_emplatado");
            kill(pid_cocina, SIGTERM);
            kill(pid_sala, SIGTERM);
            printf("[Cocina] Recursos liberados y proceso finalizado.\n");
            exit(EXIT_SUCCESS);

        }
    } else {
        /* Proceso Sala */

        // Configuramos el handler de SIGINT en el proceso sala para capturar la señal y activar el flag de salida
        struct sigaction sa_sala = {0};
        sa_sala.sa_handler = sigint_handler;
        sigemptyset(&sa_sala.sa_mask);     // No bloquea otras señales mientras ejecuta el handler
        sa_sala.sa_flags = 0;
        sigaction(SIGINT, &sa_sala, NULL);

        //Esperamos plato
        struct sigaction sa_emplatado = {0};
        sa_emplatado.sa_handler = plato_listo;
        sigemptyset(&sa_emplatado.sa_mask);  // Por seguridad, aunque no es tan crítico aquí
        sa_emplatado.sa_flags = 0;
        sigaction(SIGUSR1, &sa_emplatado, NULL);

        //memory allocation del buffer
        buffer_pedido = malloc(2048);

        //Creamos la cola POSIX
        queue = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0664, &attributes);
        if (queue == -1) {
            perror("[Sala] Error al abrir la cola");
            exit(EXIT_FAILURE);
        }

        // Arrancamos el hilo para escuchar el teclado
        if (pthread_create(&t_sala_input, NULL, &escuchar_teclado, NULL) != 0) {
            perror("[Sala] Error al crear hilo teclado");
            exit(EXIT_FAILURE);
        }else{
        }

        printf("[Sala] Inicio de la gestión de comandas...\n");
 
        // La primera vez copiamos el pedido en una variable tipo Comanda, que es aceptada por la cola y enviamos dicha Comanda, despues cerramos la cola
        strcpy(pedido.msg,"Una de champiñones");
        mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
        mq_close(queue);
 
	    int num_comanda = 0;
        //Para comprobar el numero de pedidos en cola en t0, inicia en 0, luego llega la comanda
             
        while (!finalizar) {
            while (!ready && !finalizar) pause();
            mq_getattr (queue, &attributes);                                 
            if (attributes.mq_curmsgs > 0){ 
                if (finalizar) break;
                sleep(tiempo_aleatorio(5, 10));
                printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);
            num_comanda++;
            }else{
                //sI NO HAY comandas esperamos a alguna
                while(attributes.mq_curmsgs == 0){
                    pause();
                }  
            }
           
        }
        
        // Cuando finalizar sea activado:
        pthread_join(t_sala_input, NULL);
        printf("Se va por sala");
        printf("[Sala] Proceso finalizando y recursos liberados.\n");
        exit(EXIT_SUCCESS);
    }  

    exit(0);
}