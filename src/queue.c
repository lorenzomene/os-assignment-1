#include "queue.h"
#include <stdlib.h>
#include <stdio.h> 

// Incializa uma fila
void queue_init(Queue *q){
	q->head = 0;
	q->tail = 0;
	q->count = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->can_produce, NULL);
	pthread_cond_init(&q->can_consume, NULL);
	q->producers_finished = 0;
}

// Da push em um registro para uma fila
void queue_push(Queue *q, ParsedData *data) {
	pthread_mutex_lock(&q->mutex);

	// Se uma fila ficar mais cheia que as demais isso
	// pode criar um hotstop diminuindo a eficiencia
	while (q->count == QUEUE_CAPACITY) {
		pthread_cond_wait(&q->can_produce, &q->mutex);
	}

	q->buffer[q->tail] = data;
	q->tail = (q->tail + 1) % QUEUE_CAPACITY;
	q->count++;

	pthread_cond_signal(&q->can_consume);
	pthread_mutex_unlock(&q->mutex);
}

ParsedData *queue_pop(Queue *q) {
	ParsedData *data = NULL;
	pthread_mutex_lock(&q->mutex);

	// Aguardando até que haja dados para consumir ou que os produtores tenham terminado
	while (q->count == 0 && !q->producers_finished) {
		pthread_cond_wait(&q->can_consume, &q->mutex);
	}
	
	if (q->count > 0) {
		// Retira o dado da fila
		data = q->buffer[q->head];
		q->head = (q->head + 1) % QUEUE_CAPACITY;
		q->count--;

		pthread_cond_signal(&q->can_produce);
	}else if (q->producers_finished && q->count == 0) {
		// Fila vazia e produtores terminaram, sinaliza para outros consumidores
		pthread_cond_broadcast(&q->can_consume);
	}

	pthread_mutex_unlock(&q->mutex);
	return data;
}

// Sinaliza que os produtores terminaram de adicionar dados à fila
void queue_signal_producers_finished(Queue *q) {
	pthread_mutex_lock(&q->mutex);
	q->producers_finished = 1;
	pthread_cond_broadcast(&q->can_consume);
	pthread_mutex_unlock(&q->mutex);
}

// Limpa a memoria de uma fila
void queue_destroy(Queue *q) {
	// Certifique-se que todos os elementos foram consumidos ou liberados
	for (int i = 0; i < q->count; ++i) {
		free(q->buffer[(q->head + i) % QUEUE_CAPACITY]);
	}

	// Destrói os mutexes e condition variables
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->can_produce);
	pthread_cond_destroy(&q->can_consume);
}
