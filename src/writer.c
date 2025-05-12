#include "writer.h"
#include "globals.h"
#include "types.h"  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    
#include <pthread.h>


static int compare_device_aggregates(const void *a, const void *b) {
	DeviceAggregates *da = *(DeviceAggregates **)a;
	DeviceAggregates *db = *(DeviceAggregates **)b;
	return strcmp(da->device_id, db->device_id);
}

static int compare_ano_mes_aggregates(const void *a, const void *b) {
	AnoMesAggregates *ama = (AnoMesAggregates *)a;
	AnoMesAggregates *amb = (AnoMesAggregates *)b;
	return strcmp(ama->ano_mes, amb->ano_mes);
}

void *writer_thread_func() {

	// Calcula os valores médios para cada sensor
	pthread_mutex_lock(&writer_data_mutex);
	for (size_t i = 0; i < allDevicesCount; ++i) {
		DeviceAggregates *device = allDevicesData[i];

		for (size_t j = 0; j < device->monthly_count; ++j) {
			AnoMesAggregates *month = &device->monthly_aggregates[j];

			// Computa o valor médio para cada sensor
			for (int k = 0; k < NUM_SENSORS; ++k) {
				if (month->sensor_data[k].count > 0) {
					month->sensor_data[k].avg = month->sensor_data[k].sum / month->sensor_data[k].count;
				} else {
					month->sensor_data[k].avg = 0.0;
				}
			}
		}
	}
	pthread_mutex_unlock(&writer_data_mutex);

	// Ordena os dispositivos por ID
	qsort(allDevicesData, allDevicesCount, sizeof(DeviceAggregates *), compare_device_aggregates);

	FILE *outfile = fopen(OUTPUT_FILENAME, "w");

	if (!outfile) {
		perror("Writer: Failed to open output file");
		return NULL; 
	}

	fprintf(outfile, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");

	for (size_t i = 0; i < allDevicesCount; ++i) {
		DeviceAggregates *device = allDevicesData[i];

		// Ordena os meses por ano-mes
		qsort(device->monthly_aggregates, device->monthly_count, sizeof(AnoMesAggregates), compare_ano_mes_aggregates);

		for (size_t j = 0; j < device->monthly_count; ++j) {
			AnoMesAggregates *month = &device->monthly_aggregates[j];

			for (int k = 0; k < NUM_SENSORS; k++) {
				SensorStats *sensor = &month->sensor_data[k];
				char *sensorName = SENSOR_NAMES[k];
				
				if (sensor->initialized && sensor->count > 0) {
					fprintf(outfile, "%s;%s;%s;%.2f;%.2f;%.2f\n",
							device->device_id,
							month->ano_mes,
							sensorName,
							sensor->max,
							sensor->avg,
							sensor->min);
				}
			}
		}
	}

	// Limpa a memória alocada
	fclose(outfile);

	printf("Resultados escritos em %s\n", OUTPUT_FILENAME);

	// Libera a memória alocada para os dados dos dispositivos
	pthread_mutex_lock(&writer_data_mutex);

	for (size_t i = 0; i < allDevicesCount; ++i) {
		if (allDevicesData[i]) {
			if (allDevicesData[i]->monthly_aggregates) {
				free(allDevicesData[i]->monthly_aggregates);
			}
			free(allDevicesData[i]);
		}
	}

	free(allDevicesData);
	allDevicesData = NULL;
	allDevicesCount = 0;
	allDevicesCapacity = 0;
	pthread_mutex_unlock(&writer_data_mutex);

	return NULL;
}
