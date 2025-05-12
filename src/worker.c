#include <stdio.h>
#include <stdlib.h>	
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <float.h>
#include <ctype.h>
#include <stdint.h>
#include "murmur_hash.h"
#include "globals.h"
#include "queue.h"
#include "worker.h"

void init_sensor_stats(SensorStats *stats) {
	stats->min = DBL_MAX;
	stats->max = -DBL_MAX;
	stats->sum = 0.0;
	stats->count = 0;
	stats->avg = 0.0;
	stats->initialized = 0;
}


int ano_mes_to_int(const char *ano_mes) {
	int year, month;
	sscanf(ano_mes, "%d-%d", &year, &month);
	return year * 100 + month;
}

void *worker_thread_func(void *arg) {
	long myId = (long)arg;
	Queue *myQueue = &workerQueues[myId];
	
	size_t myDevicesCount = 0;
	size_t myDevicesCapacity = 16;
	DeviceAggregates **myDevices = malloc(myDevicesCapacity * sizeof(DeviceAggregates *)); 
	if (!myDevices) {
		perror("Worker: Failed to allocate myDevices");
		return NULL;
	}

	ParsedData *data;

	while ((data = queue_pop(myQueue)) != NULL) {

		DeviceAggregates *currentDevice = NULL;

		for (size_t i = 0; i < myDevicesCount; ++i) {
			if (strcmp(myDevices[i]->device_id, data->device_id) == 0) {
				currentDevice = myDevices[i];
				break;
			}
		}

		if (currentDevice == NULL) {
			// Expande o array de dispositivos se necessário
			if (myDevicesCount >= myDevicesCapacity) {
				myDevicesCapacity = myDevicesCapacity * 2;
				DeviceAggregates **newArr = realloc(myDevices, myDevicesCapacity * sizeof(DeviceAggregates *));

				if (!newArr) {
					perror("Worker: Failed to realloc myDevices");
					free(data);
					continue;
				}
				myDevices = newArr;
			}

			currentDevice = malloc(sizeof(DeviceAggregates));

			if (!currentDevice) {
				perror("Worker: Failed to malloc DeviceAggregates");
				free(data);
				continue;
			}
		
			strncpy(currentDevice->device_id, data->device_id, DEVICE_ID_LEN - 1);
			currentDevice->device_id[DEVICE_ID_LEN - 1] = '\0';
			currentDevice->monthly_aggregates = NULL;
			currentDevice->monthly_capacity = 0;
			currentDevice->monthly_count = 0;
			currentDevice->data_outside_current_range = 0;
		
			myDevices[myDevicesCount++] = currentDevice;
		}

		AnoMesAggregates *currentMonth = NULL;

		// Procura o mês atual de processamento do dispositivo
		for (size_t i = 0; i < currentDevice->monthly_count; ++i) {
			if (strcmp(currentDevice->monthly_aggregates[i].ano_mes, data->ano_mes) == 0) {
				currentMonth = &currentDevice->monthly_aggregates[i];
				break;
			}
		}


		if (currentMonth == NULL) {
			if (currentDevice->monthly_count >= currentDevice->monthly_capacity) {
				currentDevice->monthly_capacity = (currentDevice->monthly_capacity == 0) ? 8 : currentDevice->monthly_capacity * 2;
				AnoMesAggregates *newMontlyArr = realloc(currentDevice->monthly_aggregates, currentDevice->monthly_capacity * sizeof(AnoMesAggregates));
				if (!newMontlyArr) {
					perror("Worker: Failed to realloc monthly_aggregates");
					free(data);
					continue;
				}
				currentDevice->monthly_aggregates = newMontlyArr;
			}


			currentMonth = &currentDevice->monthly_aggregates[currentDevice->monthly_count++];
			strncpy(currentMonth->ano_mes, data->ano_mes, ANO_MES_LEN - 1);
			currentMonth->ano_mes[ANO_MES_LEN - 1] = '\0';

			for (int i = 0; i < NUM_SENSORS; ++i) {
				init_sensor_stats(&currentMonth->sensor_data[i]);
			}
		}

		SensorStats *sensor = &currentMonth->sensor_data[data->sensor_type];

		if (!sensor->initialized) {
			sensor->min = data->value;
			sensor->max = data->value;
			sensor->initialized = 1;
		} else {
			if (data->value < sensor->min){
				sensor->min = data->value;
			}
			if (data->value > sensor->max) {
				sensor->max = data->value;
			}
		}
		sensor->sum += data->value;
		sensor->count++;

		free(data);
	}

	// Envia os dados do dispositivo para o escritor
    pthread_mutex_lock(&writer_data_mutex);

    for (size_t i = 0; i < myDevicesCount; ++i) {
		
		// Expande o array global se necessario
        if (allDevicesCount >= allDevicesCapacity) {
			allDevicesCapacity = allDevicesCapacity * 2;
            DeviceAggregates **newArr = realloc(allDevicesData, allDevicesCapacity * sizeof(DeviceAggregates *));
            if (!newArr) {
				perror("Worker: Failed to realloc allDevicesCapacity");
                break;
            }
            allDevicesData = newArr;
        }
        allDevicesData[allDevicesCount++] = myDevices[i];
    }
    pthread_mutex_unlock(&writer_data_mutex);

	free(myDevices);
	return NULL;
}
