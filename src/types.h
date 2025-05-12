#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <stddef.h> 
#include <float.h>


#define MAX_LINE_LEN 1024
#define DEVICE_ID_LEN 64
#define ANO_MES_LEN 8 // YYYY-MM + null terminator
#define SENSOR_NAME_LEN 60
#define OUTPUT_FILENAME "output.csv"
#define INPUT_FILENAME "devices.csv"
#define INPUT_DELIMITER "|"
#define INPUT_FILE_FIELDS 12
#define QUEUE_CAPACITY 2048

typedef enum {
	LINE_ID,
	DEVICE_ID,
	CONTAGEM,
	DATE,
	TEMPERATURE,
	HUMIDITY,
	LUMINOSITY,
	NOISE,
	ECO2,
	ETVOC,
	LATITUDE,
	LONGITUDE,
} InputFileFields;


typedef enum {
    SENSOR_TEMPERATURA,
    SENSOR_UMIDADE,
    SENSOR_LUMINOSIDADE,
    SENSOR_RUIDO,
    SENSOR_ECO2,
    SENSOR_ETVOC,
    NUM_SENSORS
} SensorType;

typedef struct {
    char device_id[DEVICE_ID_LEN];
    char ano_mes[ANO_MES_LEN];
    SensorType sensor_type;
    double value;
} ParsedData;

typedef struct {
    ParsedData *buffer[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t can_produce;
    pthread_cond_t can_consume;
    int producers_finished;
} Queue;

typedef struct {
    double min;
    double max;
    double sum;
    int count;
    double avg;
    int initialized;
} SensorStats;

typedef struct {
    char ano_mes[ANO_MES_LEN];
    SensorStats sensor_data[NUM_SENSORS];
} AnoMesAggregates;

typedef struct {
    char device_id[DEVICE_ID_LEN];
    AnoMesAggregates *monthly_aggregates;
    size_t monthly_capacity;
    size_t monthly_count;
	int current_processing_month;
	int data_outside_current_range;
} DeviceAggregates;

#endif
