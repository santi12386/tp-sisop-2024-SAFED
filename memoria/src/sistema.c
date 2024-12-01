#include "sistema.h"

extern int socket_cliente_cpu;
extern t_memoria *memoria_usuario;

void enviar_contexto(){
    t_list *paquete_recv_list;
    t_paquete *paquete_recv;
    t_paquete *paquete_send;
    int pid;
    int tid;
    int PC;

    
    paquete_recv_list = recibir_paquete(socket_cliente_cpu);

    paquete_recv = list_remove(paquete_recv_list, 0);
    pid = (int)paquete_recv->buffer->stream;
    paquete_recv = list_remove(paquete_recv_list, 0);
    tid = (int)paquete_recv->buffer->stream;
    paquete_recv = list_remove(paquete_recv_list, 0);
    PC = (int)paquete_recv->buffer->stream;

    if(pid <= 0 || tid < 0 || PC < 0){
        log_error(logger, "CPU envio parametros incorrectos pidiendo el contexto de ejecucion");
    }
    int index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if(index_pcb==-1){
        error_contexto("No se encontro el PID en memoria");
    }
    t_pcb *pcb_aux = list_get(memoria_usuario->lista_pcb, index_pcb);
    int index_thread = buscar_tid(pcb_aux->listaTCB, tid);
    if(index_thread==-1){
        error_contexto("No se encontro el TID en el proceso");
    }
    t_tcb *tcb_aux = list_get(pcb_aux->listaTCB, index_thread);

    agregar_a_paquete(paquete_send, pcb_aux->registro, sizeof(pcb_aux->registro));
    enviar_paquete(paquete_send, socket_cliente_cpu);
    eliminar_paquete(paquete_send);
}
void recibir_contexto(){

    t_list *paquete_recv_list;
    t_paquete *paquete_recv;
    t_paquete *paquete_send;
    RegistroCPU *registro;
    int pid;
    int index_pcb;
    t_pcb *aux_pcb;

    paquete_recv_list = recibir_paquete(socket_cliente_cpu);
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(registro, paquete_recv->buffer->stream, sizeof(RegistroCPU));
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(&pid, paquete_recv->buffer->stream, sizeof(int));
    
    index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if(index_pcb==-1){
        error_contexto("No se encontro el PID en memoria");
    }
    aux_pcb = list_get(memoria_usuario->lista_pcb, index_pcb);
    aux_pcb->registro = registro;

    paquete_send = crear_paquete(CONTEXTO_SEND);
    char *msj;
    strcpy(msj, "contexto recibido");
    agregar_a_paquete(paquete_send, msj, sizeof(msj));
    log_info(logger, msj);

}
//retorna index de pid en la lista de PCB
int buscar_pid(t_list *lista, int pid){
    t_pcb *elemento;
    t_list_iterator * iterator = list_iterator_create(lista);

    while(list_iterator_has_next(iterator)){
        elemento = list_iterator_next(iterator);
        if(elemento->pid == pid){
            return list_iterator_index(iterator);
        }
    }
    list_iterator_destroy(iterator);
    return -1;
}
//retorna index de tid en la lista de threads
int buscar_tid(t_list *lista, int tid){
    t_tcb *elemento;
    t_list_iterator * iterator = list_iterator_create(lista);

    while(list_iterator_has_next(iterator)){
        elemento = list_iterator_next(iterator);
        if(elemento->tid == tid){
            return list_iterator_index(iterator);
        }
    }
    list_iterator_destroy(iterator);
    return -1;
}
void error_contexto(char * error){
    log_error(logger, error);
    t_paquete *send = crear_paquete(ERROR_MEMORIA);
    enviar_paquete(send, socket_cliente_cpu);
    //agregar_a_paquete(send, error, sizeof(error)); se puede mandar error sin mensaje. Sino se complejiza el manejo de la respuesta del lado de cpu
    eliminar_paquete(send);
    return;
}
/// @brief busca un lugar vacio en la tabla y agrega el tid
/// @returns index donde guardo el tid
/// @param tcb 
int agregar_a_tabla_particion_fija(t_pcb *pcb){
    
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_particiones_fijas);
    elemento_particiones_fijas *aux;
    

    while(list_iterator_has_next(iterator)) {
        aux = list_iterator_next(iterator);
        if (aux->libre_ocupado==0){
            aux->libre_ocupado = pcb->pid; //no liberar aux, sino se pierde el elemento xd
            break;
        }
    }return list_iterator_index(iterator);
    list_iterator_destroy(iterator);
}
/// @brief busca el TID en la tabla de particiones fijas y devuelve el index
/// @param tid 
/// @return index o -1 -> error
int buscar_en_tabla_fija(int pid){
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_particiones_fijas);
    elemento_particiones_fijas *aux;
    while(list_iterator_has_next(iterator)){
        aux = list_iterator_next(iterator);
        if(aux->libre_ocupado == pid){
            return list_iterator_index(iterator);
        }
    }return -1;
}
void inicializar_tabla_particion_fija(t_list *particiones){
    elemento_particiones_fijas * aux = malloc(sizeof(elemento_particiones_fijas));
    aux->libre_ocupado = 0; // elemento libre
    aux->base = 0;
    aux->size = 0;
    uint32_t acumulador = 0;
    t_list_iterator *iterator_particiones = list_iterator_create(particiones);
    t_list_iterator *iterator_tabla = list_iterator_create(memoria_usuario->tabla_particiones_fijas);

    while(list_iterator_has_next(iterator_particiones)){
        aux->libre_ocupado = 0;
        aux->base = acumulador;
        aux->size = (int)list_iterator_next(iterator_particiones);
        acumulador += (uint32_t)aux->size;
        list_iterator_add(iterator_tabla, aux);
    }

    list_iterator_destroy(iterator_particiones);
    list_iterator_destroy(iterator_tabla);
    return;
}
void crear_proceso(t_pcb *pcb){
    int index = agregar_a_tabla_particion_fija(pcb);
    elemento_particiones_fijas *aux = list_get(memoria_usuario->tabla_particiones_fijas, index);
    pcb->registro->base=aux->base; //guarda la direccion de inicio del segmento en el registro "base"
    pcb->registro->limite=aux->size;
    //agregar pcb a la lista global
    list_add(memoria_usuario->lista_pcb, pcb);
    
    t_list_iterator *iterator = list_iterator_create(pcb->listaTCB); // iterator para listaTCB
    t_tcb * tcb_aux;
    while(list_iterator_has_next(iterator)){ // llena la lista de tcb con los tcb que vinieron dentro del pcb
        tcb_aux = list_iterator_next(iterator);
        list_add(memoria_usuario->lista_tcb, tcb_aux);
    }
}
void crear_thread(t_tcb *tcb){
    int index_pid;
    t_pcb *pcb_aux;
    index_pid = buscar_tid(memoria_usuario->lista_pcb, tcb->tid);

    pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
    list_add(pcb_aux->listaTCB, tcb);
    list_add(memoria_usuario->lista_tcb, tcb);

}
void fin_proceso(int pid){ // potencialmente faltan semaforos
    int index_pid, index_tid;
    t_pcb *pcb_aux;
    t_tcb *tcb_aux;
    switch(memoria_usuario->tipo_particion){

        case FIJAS:
            elemento_particiones_fijas *aux;
            index_pid = buscar_en_tabla_fija(pid);
            if(index_pid!=(-1)){
                aux = list_get(memoria_usuario->tabla_particiones_fijas, index_pid);
                aux->libre_ocupado = 0;
            }
            index_pid = buscar_pid(memoria_usuario->lista_pcb, pid);
            pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
            
            t_list_iterator *iterator = list_iterator_create(pcb_aux->listaTCB);
            while(list_iterator_has_next(iterator)){
                tcb_aux = list_iterator_next(iterator);
                index_tid = buscar_tid(memoria_usuario->lista_tcb, tcb_aux->tid);
                list_remove(memoria_usuario->lista_tcb, index_tid);
            }
            //mutex?
            list_remove(memoria_usuario->lista_pcb, index_pid);
            //mutex?
            
        case DINAMICAS:
            //falta hacer
            break;
    }
}
void fin_thread(int tid){
    int index_tid, index_pid;
    int pid;
    t_tcb *tcb_aux;
    t_pcb *pcb_aux;
    index_tid = buscar_tid(memoria_usuario->lista_tcb, tid);

    tcb_aux = list_get(memoria_usuario->lista_tcb, index_tid);
    pid = tcb_aux->pid;

    index_pid = buscar_pid(memoria_usuario->lista_pcb, pid);
    pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);

    t_list_iterator *iterator = list_iterator_create(pcb_aux->listaTCB);
    while(list_iterator_has_next(iterator)){
        tcb_aux = list_iterator_next(iterator);
        if (tcb_aux->tid == tid){
            list_iterator_remove(iterator);
            break;
        }
    }
    list_remove(memoria_usuario->lista_tcb, index_tid);
}
int obtener_instruccion(int PC, int tid){ // envia el paquete instruccion a cpu. Si falla, retorna -1
	if(PC<0){
        log_error(logger, "PC invalido");
		return -1;
	}
	
	t_paquete *paquete_send;
    t_tcb *tcb_aux;
	int index_tid;
	char * instruccion;
    int pid;

    index_tid = buscar_tid(memoria_usuario->lista_tcb, tid); //consigo tcb
    tcb_aux = list_get(memoria_usuario->lista_tcb, index_tid);

	instruccion = list_get(tcb_aux->instrucciones, PC);

	paquete_send = crear_paquete(OBTENER_INSTRUCCION);
	agregar_a_paquete(paquete_send, instruccion, sizeof(instruccion));
	enviar_paquete(paquete_send, socket_cliente_cpu);
	eliminar_paquete(paquete_send);
}