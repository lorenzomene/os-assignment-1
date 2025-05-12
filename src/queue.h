#ifndef QUEUE_H
#define QUEUE_H

#include "types.h" 
#include <pthread.h>

void queue_init(Queue *q);
void queue_push(Queue *q, ParsedData *data);
ParsedData *queue_pop(Queue *q);
void queue_signal_producers_finished(Queue *q);
void queue_destroy(Queue *q);

#endif
