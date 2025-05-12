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
#include "types.h"
#include "helpers.h"
#include "worker.h"
#include "writer.h"


int should_fiter_line(char *fields[INPUT_FILE_FIELDS]){
	int year, month;
	sscanf(fields[DATE], "%d-%d", &year, &month);
	if (year < 2024 || (year == 2024 && month < 3)) {
		return 1;
	}

	return 0;
}

int main() {
	printf("Iniciando o processamento...\n");

	// Identifica quantos núcleos estão disponíveis
	long coreCount = sysconf(_SC_NPROCESSORS_ONLN);
	N_WORKER_THREADS = (coreCount > 1) ? (int)(coreCount) : 1;

	printf("Usando %d threads.\n", N_WORKER_THREADS);

	// Inicializa as filas para os workers
	workerQueues = malloc(N_WORKER_THREADS * sizeof(Queue));

	if (!workerQueues) {
		perror("Failed to allocate workerQueues");
		return 1;
	}

	for (int i = 0; i < N_WORKER_THREADS; ++i) {
		queue_init(&workerQueues[i]);
	}

	// Inicializa o mutex para os dados do escritor/agregador
	if (pthread_mutex_init(&writer_data_mutex, NULL) != 0) {
		perror("Failed to initialize writer_data_mutex");
		return 1;
	}

	// Inicializa as threads de trabalho
	pthread_t workerThreads[N_WORKER_THREADS];
	pthread_t writerThread;

	allDevicesData = malloc(allDevicesCapacity * sizeof(DeviceAggregates *));
	if (!allDevicesData) {
		perror("Failed to allocate allDevicesData");
		return 1;
	}


	for (long i = 0; i < N_WORKER_THREADS; ++i) {
		if (pthread_create(&workerThreads[i], NULL, worker_thread_func, (void *)i) != 0) {
			perror("Failed to create worker thread");
			return 1;
		}
	}

	FILE *inputFile = fopen(INPUT_FILENAME, "r");

	if (!inputFile){
		perror("Failed to open input file");
		return 1;
	}


	char line[MAX_LINE_LEN];
	
	//  Pula o cabeçalho
	if (fgets(line, sizeof(line), inputFile) == NULL) {
		fprintf(stderr, "Arquivo de entrada vazio ou erro ao ler cabeçalho.\n");
		fclose(inputFile);
		return 1;
	}

	int lineCount = 0;

	while (fgets(line, sizeof(line), inputFile)) {
		lineCount++;

		char *token;
		char *fields[INPUT_FILE_FIELDS];

		// Lê cada um dos campos da linha e joga eles no array fields
		int fieldsCounter = 0;
		char *linePtr = line;
		while ((token = strsep(&linePtr, INPUT_DELIMITER)) != NULL && fieldsCounter < INPUT_FILE_FIELDS) {
			fields[fieldsCounter++] = trim_whitespace(token);
		}

		if (fieldsCounter < INPUT_FILE_FIELDS) {
			continue;
		}

		char *deviceId = fields[DEVICE_ID];

		// Filtra device_id vazios
		if (strlen(deviceId) == 0) {
			continue;
		}

		char *date = fields[DATE];	

		// Filtra data vazia
		if (strlen(date) == 0) {
			continue;
		}
		
		// Converte a data para o formato YYYY-MM
		char anoMes[ANO_MES_LEN]; 
		snprintf(anoMes, sizeof(anoMes), "%.7s", date); 

		if(should_fiter_line(fields) == 1){
			continue;
		}


		const int sensorFieldsIdx[NUM_SENSORS] = {
			TEMPERATURE, 
			HUMIDITY, 
			LUMINOSITY, 
			NOISE, 
			ECO2, 
			ETVOC
		};

		// Computa o hash do deviceId para determinar o worker thread
		// responsavel por processar os dados desse deviceId
		uint32_t deviceHash = murmur_hash(deviceId, strlen(deviceId), 0);
		int targetWorkerIdx = deviceHash % N_WORKER_THREADS;


		for (int i = 0; i < NUM_SENSORS; ++i) {
			char *sensorVal = fields[sensorFieldsIdx[i]];

			char *endptr;

			if (strlen(sensorVal) == 0) {
				continue;
			}

			double value = strtod(sensorVal, &endptr);

			// Verifica se a conversão foi bem-sucedida
			// endptr deve apontar para o final da string (ou para um espaço em branco)
			// Se sensorVal == endptr, nenhuma conversão ocorreu.
			if (sensorVal == endptr || (*endptr != 0 && !isspace((unsigned char)*endptr))) {
				continue;
			}


			ParsedData *pd = malloc(sizeof(ParsedData));
			
			if (!pd) {
				perror("Failed to malloc ParsedData for worker");
				// Lidar com erro de memória, talvez parar todas as threads e sair
				// Por simplicidade, continuamos, mas isso pode levar a dados incompletos.
				continue;
			}

			
			// Copia os dados para a estrutura ParsedData
			strncpy(pd->device_id, deviceId, DEVICE_ID_LEN - 1);
			pd->device_id[DEVICE_ID_LEN - 1] = '\0';

			strncpy(pd->ano_mes, anoMes, ANO_MES_LEN - 1);
			pd->ano_mes[ANO_MES_LEN - 1] = '\0';
			
			pd->sensor_type = (SensorType) i;
			pd->value = value;

			// Envia os dados para a fila do worker correspondente
			queue_push(&workerQueues[targetWorkerIdx], pd);
		}
	}

	fclose(inputFile);
	printf("Arquivo de entrada processado. Aguardando finalização das threads...\n");

	// Sinalizar que a produção para as worker queues terminou
	for (int i = 0; i < N_WORKER_THREADS; ++i) {
		queue_signal_producers_finished(&workerQueues[i]);
	}

	// Esperar workers terminarem
	for (int i = 0; i < N_WORKER_THREADS; ++i) {
		pthread_join(workerThreads[i], NULL);
		queue_destroy(&workerQueues[i]);
	}

	free(workerQueues);

	// Cria a thread do escritor 
	if (pthread_create(&writerThread, NULL, writer_thread_func, NULL) != 0) {
		perror("Failed to create writer thread");
		return 1;
	}

	// Esperar writer thread terminar
	pthread_join(writerThread, NULL);
	
	pthread_mutex_destroy(&writer_data_mutex);

	printf("Processamento concluído.\n");

	return 0;
}
