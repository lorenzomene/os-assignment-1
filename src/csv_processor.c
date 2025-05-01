#define _POSIX_C_SOURCE 200809L
#include "csv_processor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --------- helpers de tabela --------- */

static void table_init(StatsTable *t, size_t cap)
{
    t->entries  = malloc(cap * sizeof(StatsEntry));
    t->size     = 0;
    t->capacity = cap;
}

static void table_grow(StatsTable *t)
{
    t->capacity *= 2;
    t->entries   = realloc(t->entries, t->capacity * sizeof(StatsEntry));
}

static int entry_matches(const StatsEntry *e,
                         const char *device, int year, int month)
{
    return (e->year == year && e->month == month && strcmp(e->device, device) == 0);
}

/* Retorna ponteiro p/ entry existente ou recém-criada. */
static StatsEntry *table_get_or_create(StatsTable *t,
                                       const char *device, int year, int month)
{
    for (size_t i = 0; i < t->size; ++i)
        if (entry_matches(&t->entries[i], device, year, month))
            return &t->entries[i];

    /* não achou → criar nova */
    if (t->size == t->capacity) table_grow(t);

    StatsEntry *e = &t->entries[t->size++];
    strncpy(e->device, device, KEY_MAX);
    e->device[KEY_MAX - 1] = '\0';
    e->year  = year;
    e->month = month;
    for (int s = 0; s < SENSOR_COUNT; ++s) {
        e->sensor[s].max   = -1e308;   /* –inf */
        e->sensor[s].min   = +1e308;   /* +inf */
        e->sensor[s].sum   = 0.0;
        e->sensor[s].count = 0;
    }
    return e;
}

/* --------- parsing linha CSV --------- */

static void parse_line(const char *line,
                       char *device, int *year, int *month,
                       double value[SENSOR_COUNT])
{
    /* Espera formato id;device;contagem;data;temp;umid;lum;ruido;eco2;etvoc;lat;lon */
    /*            0   1      2         3    4    5     6   7      8    9    10  11 */
    char *dup = strdup(line);
    char *tok;
    int   col = 0;

    for (tok = strtok(dup, ";"); tok; tok = strtok(NULL, ";"), ++col) {
        switch (col) {
        case 1: strncpy(device, tok, KEY_MAX-1); device[KEY_MAX-1]='\0'; break;
        case 3: sscanf(tok, "%d-%d", year, month); break; /* ano-mes-dia */
        case 4: value[0] = atof(tok); break;
        case 5: value[1] = atof(tok); break;
        case 6: value[2] = atof(tok); break;
        case 7: value[3] = atof(tok); break;
        case 8: value[4] = atof(tok); break;
        case 9: value[5] = atof(tok); break;
        default: break;
        }
    }
    free(dup);
}

/* --------- thread worker --------- */

static void *thread_fn(void *arg)
{
    ThreadTask *task = arg;
    table_init(&task->local_table, 64);

    for (size_t i = 0; i < task->count; ++i) {
        char device[KEY_MAX];
        int year=0, month=0;
        double val[SENSOR_COUNT] = {0};

        parse_line(task->lines[i], device, &year, &month, val);

        /* ignorar registros anteriores a 2024-03 */
        if (year < 2024 || (year == 2024 && month < 3))
            continue;

        StatsEntry *e = table_get_or_create(&task->local_table, device, year, month);
        for (int s = 0; s < SENSOR_COUNT; ++s) {
            SensorStats *st = &e->sensor[s];
            if (val[s] > st->max) st->max = val[s];
            if (val[s] < st->min) st->min = val[s];
            st->sum   += val[s];
            st->count += 1;
        }
    }
    return NULL;
}

/* --------- API implementations --------- */

char **load_csv(const char *path, size_t *line_count)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("devices.csv"); exit(EXIT_FAILURE); }

    size_t cap = 1024, n = 0;
    char **lines = malloc(cap * sizeof(char *));
    char *buf = NULL;
    size_t len = 0;

    getline(&buf, &len, f); /* descarta cabeçalho */
    while (getline(&buf, &len, f) != -1) {
        if (n == cap) {
            cap *= 2;
            lines = realloc(lines, cap * sizeof(char *));
        }
        lines[n++] = strdup(buf);
    }
    free(buf);
    fclose(f);
    *line_count = n;
    return lines;
}

StatsTable process_csv_mt(char **all_lines, size_t line_count)
{
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) nproc = 2;  /* fallback */

    pthread_t *threads = malloc(nproc * sizeof(pthread_t));
    ThreadTask *tasks  = calloc(nproc, sizeof(ThreadTask));

    /* divide linhas igualmente */
    size_t chunk = (line_count + nproc - 1) / nproc;
    for (long t = 0; t < nproc; ++t) {
        size_t start = t * chunk;
        size_t end   = (start + chunk > line_count) ? line_count : start + chunk;
        tasks[t].lines = &all_lines[start];
        tasks[t].count = end - start;
        pthread_create(&threads[t], NULL, thread_fn, &tasks[t]);
    }

    /* merge das tabelas locais */
    StatsTable global;
    table_init(&global, 128);

    for (long t = 0; t < nproc; ++t) {
        pthread_join(threads[t], NULL);
        StatsTable *lt = &tasks[t].local_table;
        for (size_t i = 0; i < lt->size; ++i) {
            StatsEntry *le = &lt->entries[i];
            StatsEntry *ge = table_get_or_create(&global,
                              le->device, le->year, le->month);
            for (int s = 0; s < SENSOR_COUNT; ++s) {
                SensorStats *gs = &ge->sensor[s];
                SensorStats *ls = &le->sensor[s];

                if (ls->max > gs->max) gs->max = ls->max;
                if (ls->min < gs->min) gs->min = ls->min;
                gs->sum   += ls->sum;
                gs->count += ls->count;
            }
        }
        free(lt->entries);
    }

    free(tasks);
    free(threads);
    return global;
}

void write_results(const StatsTable *table, const char *dir)
{
    mkdir(dir, 0755);            /* ok se já existir                    */
    char path[256];
    snprintf(path, sizeof(path), "%s/results.csv", dir);

    FILE *f = fopen(path, "w");
    if (!f) { perror("results.csv"); exit(EXIT_FAILURE); }

    fprintf(f, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");

    const char *sensor_name[SENSOR_COUNT] = {
        "temperatura", "umidade", "luminosidade",
        "ruido", "eco2", "etvoc"
    };

    for (size_t i = 0; i < table->size; ++i) {
        const StatsEntry *e = &table->entries[i];
        for (int s = 0; s < SENSOR_COUNT; ++s) {
            const SensorStats *st = &e->sensor[s];
            if (st->count == 0) continue;

            double avg = st->sum / st->count;
            fprintf(f, "%s;%04d-%02d;%s;%.2f;%.2f;%.2f\n",
                    e->device, e->year, e->month,
                    sensor_name[s], st->max, avg, st->min);
        }
    }
    fclose(f);
}

void free_csv_lines(char **lines, size_t line_count)
{
    for (size_t i = 0; i < line_count; ++i) free(lines[i]);
    free(lines);
}

void free_table(StatsTable *table)
{
    free(table->entries);
    table->entries = NULL;
    table->size = table->capacity = 0;
}
