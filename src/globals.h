#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h" 

extern int N_WORKER_THREADS;

extern Queue *workerQueues;

extern DeviceAggregates **allDevicesData;
extern size_t allDevicesCapacity;
extern size_t allDevicesCount;
extern pthread_mutex_t writer_data_mutex;

extern const char *SENSOR_NAMES[NUM_SENSORS];
#endif
