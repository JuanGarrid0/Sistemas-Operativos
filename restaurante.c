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
char *buffer_pedido;  

//Flag global para finalizar procesos
volatile sig_atomic_t finalizar = 0;

//Señal que protege la flag global de finalizado
void sigint_handler(int sig) {
    finalizar = 1;
    sem_post(&sem_preparado);
    sem_post(&sem_cocinado);
    sem_post(&sem_emplatado);
}


//buffer para los pedidos

void plato_listo(int sig){
        ready = 1;
 }

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

//Hilo para preparado
void* preparar_ingredientes(void* args) {
    printf("[Preparación] Hilo iniciado.\n");
    char buffer[128];
    strcpy(buffer, (char*)args);

    while (!finalizar) {
        sem_wait(&sem_preparado);
        if (finalizar) break;  // Comprobación inmediata post sem_wait

        printf("[Preparación] Preparando ingredientes...\n");
        sleep(tiempo_aleatorio(3, 6));
        printf("[Preparación] Ingredientes listos.\n");

        sem_post(&sem_cocinado);
    }

    printf("[Preparación] Hilo finalizando.\n");
    return NULL;
}


//Hilo para cocinar
void* cocinar(void* arg) {
    while (!finalizar) {
        sem_wait(&sem_cocinado);
        if (finalizar) break;  // Comprobación rápida por si la señal llega mientras espera semáforo
        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
        printf("[Cocina] Plato cocinado.\n");
        sem_post(&sem_emplatado);
    }
    printf("[Cocina] Hilo finalizando.\n");
    return NULL;
}


// Hilo para el emplatado
void* emplatar(void* arg) {

    while (!finalizar) {
        sem_wait(&sem_emplatado);   
        if (finalizar) break;                                                                    //Rojo
        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
   
        printf("[Emplatado] Plato listo y emplatado.\n");
         
       
        sem_post(&sem_preparado);                                                                       //Verde
        kill(pid_sala, SIGUSR1);


    }
    printf("[Emplatado] Hilo finalizando.\n");
    return NULL;
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

void* escuchar_teclado(void* arg) {
    while (!finalizar) {
        int c = getchar();
        if (c == 16) {  // ASCII de Ctrl+P
            struct mq_attr attr;
            mq_getattr(queue, &attr);
            if (attr.mq_curmsgs >= attr.mq_maxmsg) {
                printf("[Sala] La sala está saturada, intentelo de nuevo más tarde.\n");
            } else {
                strcpy(pedido.msg, "Pedido adicional por teclado");
                mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
                printf("[Sala] Pedido añadido desde teclado.\n");
            }
        }
        sleep(100000);
    }
    return NULL;
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
            printf("ESTOOOOOOOY");
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

            struct sigaction sa_cocina = {0};
            sa_cocina.sa_handler = sigint_handler;
            sigemptyset(&sa_cocina.sa_mask);
            sa_cocina.sa_flags = 0;
            sigaction(SIGINT, &sa_cocina, NULL);

            //Abrimos la cola en modo lectura y guardamos en el buffer
            // Abrimos la cola en modo lectura y guardamos en el buffer
            queue = mq_open(MQ_NAME, O_RDONLY);
            if (queue == -1) {
                perror("[Cocina] Error al abrir la cola");
                exit(EXIT_FAILURE);
            }

            // Intentamos recibir el pedido
            if (mq_receive(queue, buffer_pedido, attributes.mq_msgsize, 0) == -1) {
                perror("[Cocina] Error al recibir pedido");
            } else {
                //Comprobamos si hay algun fallo que haga que el dato de la cola no llegue al buffer
                printf("[Cocina] Pedido recibido: %s\n", buffer_pedido);
            }
            

            printf("[Cocina] Comienzo de la preparación de platos...\n");
            /*Arrancamos los hilos de cada uno de los procesos uno a uno, y esperamos a que acabe el ultimo para cerrar los hilos,
             de esta forma los pedidos se hacen de uno en uno. Ejemplo: Se prepara, cocina y emplata A, se prepara, cocina y emplata B */
             if (pthread_create(&t1, NULL, &preparar_ingredientes, buffer_pedido) != 0) {
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

            // Bucle principal de espera a señal de finalización
            while (!finalizar) {
                pause();  // Esperamos a recibir SIGINT
            }


            // Una vez recibido SIGINT, aseguramos que desbloqueamos los hilos si estuvieran esperando semáforos
            sem_post(&sem_preparado);
            sem_post(&sem_cocinado);
            sem_post(&sem_emplatado);

            // Esperamos a que los hilos terminen ordenadamente
            pthread_join(t3, NULL);
            printf("[INFO] Hilo Emplatado finalizado.\n");
            pthread_join(t2, NULL);
            printf("[INFO] Hilo Cocinado finalizado.\n");
            pthread_join(t1, NULL);
            printf("[INFO] Hilo Preparación finalizado.\n");


            //Al final de cada comanda liberamos el buffer y cerramos cola--> Cerrar y liberar mas arriba si se quiere hacer mas de un pedido cada vez
            free(buffer_pedido);
            mq_close (queue);
            sem_destroy(&sem_preparado);
            sem_destroy(&sem_cocinado);
            sem_destroy(&sem_emplatado);

            printf("[Cocina] Recursos liberados y proceso finalizado.\n");
            kill(getppid(),SIGTERM); //acabamos con el proceso una vez todo ha acabado
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

        //Creamos la cola POSIX
        queue = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0664, &attributes);
        if (queue == -1) {
            perror("[Sala] Error al abrir la cola");
            exit(EXIT_FAILURE);
        }

        // Arrancamos el hilo para escuchar el teclado
        pthread_t t_sala_input;
        if (pthread_create(&t_sala_input, NULL, escuchar_teclado, NULL) != 0) {
            perror("[Sala] Error al crear hilo teclado");
            exit(EXIT_FAILURE);
        }

        printf("[Sala] Inicio de la gestión de comandas...\n");



        // copiamos el pedido en una variable tipo Comanda, que es aceptada por la cola y enviamos dicha Comanda, despues cerramos la cola
        strcpy(pedido.msg,"Una de champiñones");
        mq_send(queue, (char *)&pedido, sizeof(pedido), 1);
        mq_close(queue);
 
	    int num_comanda = 0;
        //Para comprobar el numero de pedidos en cola en t0, inicia en 0, luego llega la comanda
        mq_getattr (queue, &attributes);                                //borrable
        printf ("%ld pedidos en cocina.\n", attributes.mq_curmsgs);      //borrable, saca numero de mensajes en cola
        while (!finalizar) {
            while (!ready && !finalizar) pause();
            if (finalizar) break;
            sleep(tiempo_aleatorio(5, 10));
            printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);
            num_comanda++;
            ready = 0;
        }
        
        // Cuando finalizar sea activado:
        pthread_join(t_sala_input, NULL);
        mq_close(queue);
        
        printf("[Sala] Proceso finalizando y recursos liberados.\n");
        exit(EXIT_SUCCESS);
    }  

    exit(0);
}