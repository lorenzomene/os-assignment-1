#ifndef CSV_PROCESSOR_H
#define CSV_PROCESSOR_H

#include <pthread.h>

#define SENSOR_COUNT 6           /* temp, umid, lumin, ruido, eco2, etvoc */
#define KEY_MAX      64          /* tamanho máximo do nome do device       */

typedef struct {
    double max;
    double min;
    double sum;
    unsigned long count;
} SensorStats;

typedef struct {
    char  device[KEY_MAX];
    int   year;
    int   month;                 /* 1-12                                   */
    SensorStats sensor[SENSOR_COUNT];
} StatsEntry;

/* ---------- API ---------- */
typedef struct {
    StatsEntry *entries;
    size_t      size;
    size_t      capacity;
} StatsTable;

typedef struct {
    char  **lines;               /* fatia do CSV para esta thread          */
    size_t count;
    StatsTable local_table;
} ThreadTask;

/* Le arquivo, cria vetor de linhas em memória (termina com NULL).      */
char **load_csv(const char *path, size_t *line_count);

/* Prepara tarefas e dispara N threads; devolve tabela final fundida.   */
StatsTable process_csv_mt(char **all_lines, size_t line_count);

/* Grava resultado em CSV (cria diretório se não existir).              */
void write_results(const StatsTable *table, const char *dir);

/* Libera memória alocada.                                              */
void free_csv_lines(char **lines, size_t line_count);
void free_table(StatsTable *table);

#endif /* CSV_PROCESSOR_H */
