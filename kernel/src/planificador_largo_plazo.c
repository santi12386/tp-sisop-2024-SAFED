#include <planificador_largo_plazo.h>
#include <utils/utils.h>
#include <main.h>

extern t_cola_proceso* procesos_cola_ready;
extern pthread_mutex_t * mutex_procesos_cola_ready;
extern sem_t * sem_estado_procesos_cola_ready;

extern t_cola_procesos_a_crear* procesos_a_crear;
extern pthread_mutex_t * mutex_procesos_a_crear;
extern sem_t * sem_estado_procesos_a_crear;

//Tengo que inicializarlos
extern t_list * lista_t_peticiones;
extern pthread_mutex_t * mutex_lista_t_peticiones;
extern sem_t * sem_lista_t_peticiones; 

extern pthread_mutex_t * mutex_respuesta_desde_memoria;
extern pthread_cond_t * cond_respuesta_desde_memoria;

extern sem_t * sem_proceso_finalizado;

t_cola_proceso* inicializar_cola_procesos_ready(){
    procesos_cola_ready = malloc(sizeof(t_cola_procesos_a_crear));
    if (procesos_cola_ready == NULL) {
        perror("Error al asignar memoria para cola de NEW");
        exit(EXIT_FAILURE);
    }

    procesos_cola_ready->nombre_estado = READY;
    procesos_cola_ready->lista_procesos = list_create();  // Crea una lista vacía
    if (procesos_cola_ready->lista_procesos == NULL) {
        perror("Error al crear lista de procesos en NEW");
        exit(EXIT_FAILURE);
    }
    return procesos_cola_ready;
}

t_cola_procesos_a_crear* inicializar_cola_procesos_a_crear(){
    procesos_a_crear = malloc(sizeof(t_cola_procesos_a_crear));
    if (procesos_a_crear == NULL) {
        perror("Error al asignar memoria para cola de NEW");
        exit(EXIT_FAILURE);
    }

    cola->nombre_estado = NEW;
    cola->lista_procesos = list_create();  // Crea una lista vacía
    if (cola->lista_procesos == NULL) {
        perror("Error al crear lista de procesos en NEW");
        exit(EXIT_FAILURE);
    }
    return procesos_a_crear;
}

void inicializar_semaforos_largo_plazo(){
    mutex_procesos_cola_ready = malloc(sizeof(pthread_mutex_t));
    if (mutex_procesos_cola_ready == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_procesos_cola_ready, NULL);

    mutex_procesos_a_crear = malloc(sizeof(pthread_mutex_t));
    if (mutex_procesos_a_crear == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_procesos_a_crear, NULL);
    
    sem_estado_procesos_cola_ready = malloc(sizeof(sem_t));
    if (sem_estado_procesos_cola_ready == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_procesos_cola_ready, 0, 0);

    sem_estado_procesos_a_crear = malloc(sizeof(sem_t));
    if (sem_estado_procesos_a_crear == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_procesos_a_crear, 0, 0);
}

void largo_plazo_fifo()
{
    while (1)
    {
        sem_wait(procesos_a_crear);
        pthread_mutex_lock(mutex_procesos_a_crear);
        t_pcb *proceso = desencolar_proceso_a_crear();
    // Crear petición
        t_peticion *peticion = malloc(sizeof(t_peticion));
        peticion->tipo = CREATE_PROCESS;
        peticion->proceso = proceso;
        peticion->hilo = NULL; // No aplica en este caso
        peticion->respuesta_recibida = false;
        encolar_peticion_memoria(peticion);
        if (peticion->respuesta_exitosa) {
            proceso->estado = READY;
            encolar_proceso_en_ready(proceso);
            encolar_corto_plazo(proceso);
        } else {
            log_info(logger, "Memoria no tiene espacio suficiente, reintentando cuando un proceso se termine...");
            //Se tiene que reencolar el proceso porque ya fue removido y hacerlo en el tope de la cola
            pthread_mutex_lock(mutex_procesos_a_crear);
            list_add_in_index(procesos_a_crear, 0, proceso);
            pthread_mutex_unlock(mutex_procesos_a_crear);
            sem_post(procesos_a_crear);

            // Esperar a que un proceso finalice para volver a intentar la peticion
            sem_wait(&sem_proceso_finalizado);
        }
        free(peticion);
    }
    
}

t_pcb* desencolar_proceso_a_crear(){
    t_pcb *proceso= list_remove(hilos_cola_ready->lista_hilos, 0);
    return proceso;
}

void encolar_proceso_en_ready(t_pcb * proceso){
    pthread_mutex_lock(mutex_procesos_cola_ready);
    list_add(procesos_cola_ready->lista_procesos, proceso);
    pthread_mutex_unlock(mutex_procesos_cola_ready);
    sem_post(sem_estado_procesos_cola_ready);
}

void encolar_corto_plazo(t_pcb * proceso){
    char *algoritmo = "FIFO";
    //obtener el hilo y mandarlo a encolar
    if (strcmp(algoritmo, "FIFO") == 0) {
        encolar_corto_plazo_fifo();
    } else if (strcmp(algoritmo, "PRIORIDADES") == 0) {
        encolar_corto_plazo_prioridades();
    } else if (strcmp(algoritmo, "CMN") == 0) {
        encolar_corto_plazo_colas_multinivel();
    } else {
        printf("Error: Algoritmo no reconocido.\n");
    }
}

/*
Planificador Largo Plazo
    -> Finalizacion de procesos
        -> Se le avisa a memoria (que confirma), se libera el PCB asociado (donde se almacenan?) y 
        se intenta inicializar otro proceso
    -> Creación de hilos
        -> Kernel avisa a memoria y lo ingresa a la cola de Ready según su prioridad 
        (se una lista global de TCBs ordenada por prioridades)
    -> Finalizar hilos
        -> Se le informa a memoria se libera el TCB asociado (se tiene una lista global de TCBs) 
            y se desbloquean todos los hilos bloqueados por este hilo
            ahora finalizado
*/
