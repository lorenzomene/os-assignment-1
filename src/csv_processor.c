#define _POSIX_C_SOURCE 200809L
#include "csv_processor.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------- helpers tabela ---------- */

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
                         const char *device, int y, int m)
{
    return e->year == y && e->month == m &&
           strcmp(e->device, device) == 0;
}

static StatsEntry *table_get_or_create(StatsTable *t,
                                       const char *device, int y, int m)
{
    for (size_t i = 0; i < t->size; ++i)
        if (entry_matches(&t->entries[i], device, y, m))
            return &t->entries[i];

    if (t->size == t->capacity) table_grow(t);

    StatsEntry *e = &t->entries[t->size++];
    strncpy(e->device, device, KEY_MAX - 1);
    e->device[KEY_MAX - 1] = '\0';
    e->year  = y;
    e->month = m;
    for (int s = 0; s < SENSOR_COUNT; ++s) {
        e->sensor[s].max = -HUGE_VAL;
        e->sensor[s].min =  HUGE_VAL;
        e->sensor[s].sum =  0.0;
        e->sensor[s].count = 0;
    }
    return e;
}

/* ---------- conversores seguros ---------- */

/* vírgula → ponto */
static inline void comma_to_dot(char *s)
{
    for (; *s; ++s) if (*s == ',') *s = '.';
}

/* atof minimalista (aceita apenas [+|-]DDD[.DDD]) */
static int simple_atof(const char *s, double *out)
{
    const char *p = s;
    int sign = 1;

    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; ++p; }

    if (!isdigit((unsigned char)*p)) return 0;

    long long int_part = 0;
    while (isdigit((unsigned char)*p)) {
        int_part = int_part * 10 + (*p - '0');
        ++p;
    }

    double frac = 0.0, scale = 1.0;
    if (*p == '.') {
        ++p;
        if (!isdigit((unsigned char)*p)) return 0;    /* exige dígito após '.' */

        while (isdigit((unsigned char)*p)) {
            frac  = frac * 10 + (*p - '0');
            scale *= 10.0;
            ++p;
        }
    }

    while (isspace((unsigned char)*p)) ++p;
    if (*p != '\0') return 0;                         /* lixo extra */

    *out = sign * (int_part + frac / scale);
    return 1;
}

/* ---------- parse linha ---------- */

static void parse_line(const char *line,
                       char *device, int *year, int *month,
                       double val[SENSOR_COUNT], int prs[SENSOR_COUNT])
{
    for (int i = 0; i < SENSOR_COUNT; ++i) { val[i] = 0.0; prs[i] = 0; }

    char *dup = strdup(line), *tok;
    int col = 0, pipe_count = 0;

    for (char *tmp = dup; *tmp; ++tmp) if (*tmp == '|') ++pipe_count;
    if (pipe_count < 11) { free(dup); return; }        /* linha inválida */

    for (tok = strtok(dup, "|"); tok; tok = strtok(NULL, "|"), ++col) {
        if (*tok == '\0') continue;                    /* campo vazio */

        switch (col) {
        case 1:                                        /* device */
            strncpy(device, tok, KEY_MAX - 1);
            device[KEY_MAX - 1] = '\0';
            break;

        case 3: {                                      /* data */
            if (sscanf(tok, "%d-%d", year, month) != 2) {
                int d;
                sscanf(tok, "%d/%d/%d", &d, month, year);
            }
            break;
        }

        case 4 ... 9: {                                /* sensores */
            comma_to_dot(tok);
            double tmp;
            if (simple_atof(tok, &tmp)) {
                int idx = col - 4;
                val[idx] = tmp;
                prs[idx] = 1;
            }
            break;
        }
        default: break;
        }
    }
    free(dup);
}

/* ---------- thread worker ---------- */

static void *thread_fn(void *arg)
{
    ThreadTask *t = arg;
    table_init(&t->local_table, 64);

    for (size_t i = 0; i < t->count; ++i) {
        char device[KEY_MAX] = {0};
        int  y = 0, m = 0;
        double v[SENSOR_COUNT];
        int    p[SENSOR_COUNT];

        parse_line(t->lines[i], device, &y, &m, v, p);

        if (y < 2024 || (y == 2024 && m < 3) || device[0] == '\0')
            continue;

        StatsEntry *e = table_get_or_create(&t->local_table, device, y, m);

        for (int s = 0; s < SENSOR_COUNT; ++s) {
            if (!p[s]) continue;
            SensorStats *st = &e->sensor[s];
            if (v[s] > st->max) st->max = v[s];
            if (v[s] < st->min) st->min = v[s];
            st->sum   += v[s];
            st->count += 1;
        }
    }
    return NULL;
}

/* ---------- API ---------- */

char **load_csv(const char *path, size_t *nlines)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(EXIT_FAILURE); }

    size_t cap = 2048, n = 0;
    char **lines = malloc(cap * sizeof(char *));
    char *buf = NULL; size_t len = 0;

    getline(&buf, &len, f);                     /* cabeçalho */
    while (getline(&buf, &len, f) != -1) {
        if (n == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
        lines[n++] = strdup(buf);
    }
    free(buf); fclose(f);
    *nlines = n;
    return lines;
}

StatsTable process_csv_mt(char **all_lines, size_t nlines)
{
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) nproc = 2;

    pthread_t  *thr  = malloc(nproc * sizeof(pthread_t));
    ThreadTask *task = calloc(nproc, sizeof(ThreadTask));

    size_t chunk = (nlines + nproc - 1) / nproc;
    for (long t = 0; t < nproc; ++t) {
        size_t a = t * chunk;
        size_t b = (a + chunk > nlines) ? nlines : a + chunk;
        task[t].lines = &all_lines[a];
        task[t].count = b - a;
        pthread_create(&thr[t], NULL, thread_fn, &task[t]);
    }

    StatsTable g; table_init(&g, 128);

    for (long t = 0; t < nproc; ++t) {
        pthread_join(thr[t], NULL);
        StatsTable *lt = &task[t].local_table;
        for (size_t i = 0; i < lt->size; ++i) {
            StatsEntry *le = &lt->entries[i];
            StatsEntry *ge = table_get_or_create(&g, le->device, le->year, le->month);
            for (int s = 0; s < SENSOR_COUNT; ++s) {
                const SensorStats *ls = &le->sensor[s];
                SensorStats *gs = &ge->sensor[s];
                if (!ls->count) continue;
                if (ls->max > gs->max) gs->max = ls->max;
                if (ls->min < gs->min) gs->min = ls->min;
                gs->sum   += ls->sum;
                gs->count += ls->count;
            }
        }
        free(lt->entries);
    }
    free(thr); free(task);
    return g;
}

void write_results(const StatsTable *t, const char *dir)
{
    mkdir(dir, 0755);
    char path[256]; snprintf(path, sizeof(path), "%s/results.csv", dir);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(EXIT_FAILURE); }

    fprintf(f, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");
    const char *name[SENSOR_COUNT] = { "temperatura","umidade","luminosidade",
                                       "ruido","eco2","etvoc" };

    for (size_t i = 0; i < t->size; ++i)
        for (int s = 0; s < SENSOR_COUNT; ++s) {
            const SensorStats *st = &t->entries[i].sensor[s];
            if (!st->count) continue;
            fprintf(f, "%s;%04d-%02d;%s;%.2f;%.2f;%.2f\n",
                    t->entries[i].device, t->entries[i].year, t->entries[i].month,
                    name[s], st->max, st->sum / st->count, st->min);
        }
    fclose(f);
}

void free_csv_lines(char **l, size_t n) { for (size_t i = 0; i < n; ++i) free(l[i]); free(l); }
void free_table(StatsTable *t) { free(t->entries); }
