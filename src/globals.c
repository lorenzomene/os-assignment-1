#include "globals.h"
#include <pthread.h>

int N_WORKER_THREADS;
Queue *workerQueues = NULL; 

DeviceAggregates **allDevicesData = NULL; 
size_t allDevicesCapacity = 16;
size_t allDevicesCount = 0;

pthread_mutex_t writer_data_mutex;
int workers_finished = 0;

const char *SENSOR_NAMES[NUM_SENSORS] = {
    "temperatura", "umidade", "luminosidade", "ruido", "eco2", "etvoc"};
