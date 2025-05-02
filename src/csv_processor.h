#ifndef CSV_PROCESSOR_H
#define CSV_PROCESSOR_H

#include <pthread.h>
#include <stddef.h>

#define SENSOR_COUNT 6          /* temp, umid, lumin, ruido, eco2, etvoc */
#define KEY_MAX      64

typedef struct {
    double max, min, sum;
    unsigned long count;
} SensorStats;

typedef struct {
    char device[KEY_MAX];
    int  year, month;           /* AAAA, 1-12 */
    SensorStats sensor[SENSOR_COUNT];
} StatsEntry;

/* --------- structs internas --------- */
typedef struct { StatsEntry *entries; size_t size, capacity; } StatsTable;
typedef struct { char **lines; size_t count; StatsTable local_table; } ThreadTask;

/* --------- API --------- */
char **load_csv(const char *path, size_t *line_count);
StatsTable process_csv_mt(char **all_lines, size_t line_count);
void write_results(const StatsTable *table, const char *dir);
void free_csv_lines(char **lines, size_t n);
void free_table(StatsTable *t);

#endif
