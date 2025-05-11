#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_DEVICE_NAME 100
#define SENSOR_COUNT 6

typedef struct
{
    double min;
    double max;
    double sum;
    int count;
} SensorStats;

typedef struct
{
    char device[MAX_DEVICE_NAME];
    int year;
    int month;
    SensorStats sensors[SENSOR_COUNT];
} DeviceStats;

typedef struct
{
    DeviceStats *entries;
    int size;
    int capacity;
} StatsTable;

void init_stats_table(StatsTable *table, int initial_capacity)
{
    table->entries = malloc(initial_capacity * sizeof(DeviceStats));
    table->size = 0;
    table->capacity = initial_capacity;
}

DeviceStats *get_or_create_device_stats(StatsTable *table, const char *device, int year, int month)
{
    for (int i = 0; i < table->size; i++)
    {
        if (strcmp(table->entries[i].device, device) == 0 &&
            table->entries[i].year == year &&
            table->entries[i].month == month)
        {
            return &table->entries[i];
        }
    }

    if (table->size >= table->capacity)
    {
        table->capacity *= 2;
        table->entries = realloc(table->entries, table->capacity * sizeof(DeviceStats));
    }

    DeviceStats *new_entry = &table->entries[table->size++];
    strncpy(new_entry->device, device, MAX_DEVICE_NAME - 1);
    new_entry->device[MAX_DEVICE_NAME - 1] = '\0';
    new_entry->year = year;
    new_entry->month = month;

    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        new_entry->sensors[i].min = 999999.0;
        new_entry->sensors[i].max = -999999.0;
        new_entry->sensors[i].sum = 0.0;
        new_entry->sensors[i].count = 0;
    }

    return new_entry;
}

int is_empty(const char *str)
{
    if (!str)
        return 1;
    while (*str)
    {
        if (!isspace((unsigned char)*str))
            return 0;
        str++;
    }
    return 1;
}

int is_valid_date(int year, int month)
{
    if (year < 2024)
        return 0;
    if (year == 2024 && month < 3)
        return 0;
    if (month < 1 || month > 12)
        return 0;
    return 1;
}

int is_valid_sensor_value(const char *str)
{
    if (!str || is_empty(str))
        return 0;

    char *endptr;
    strtod(str, &endptr);
    return *endptr == '\0' || isspace((unsigned char)*endptr);
}

int process_line(const char *line, StatsTable *table)
{
    char *line_copy = strdup(line);
    if (!line_copy)
        return 0;

    char *fields[12] = {NULL};
    char *token = strtok(line_copy, "|\n");
    int field_count = 0;

    while (token && field_count < 12)
    {
        fields[field_count++] = token;
        token = strtok(NULL, "|\n");
    }

    if (field_count != 12)
    {
        free(line_copy);
        return 0;
    }

    if (!fields[1] || !fields[3] || !*fields[1] || !*fields[3])
    {
        free(line_copy);
        return 0;
    }

    int year, month;
    if (sscanf(fields[3], "%d-%d", &year, &month) != 2)
    {
        free(line_copy);
        return 0;
    }

    if (year < 2024 || (year == 2024 && month < 3))
    {
        free(line_copy);
        return 0;
    }

    for (int i = 4; i <= 9; i++)
    {
        if (!fields[i] || !*fields[i] || is_empty(fields[i]))
        {
            free(line_copy);
            return 0;
        }
    }

    double sensors[SENSOR_COUNT];
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        sensors[i] = atof(fields[i + 4]);
    }

    DeviceStats *stats = get_or_create_device_stats(table, fields[1], year, month);

    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        if (sensors[i] < stats->sensors[i].min)
            stats->sensors[i].min = sensors[i];
        if (sensors[i] > stats->sensors[i].max)
            stats->sensors[i].max = sensors[i];
        stats->sensors[i].sum += sensors[i];
        stats->sensors[i].count++;
    }

    free(line_copy);
    return 1;
}

void write_results(const StatsTable *table, const char *output_file)
{
    FILE *f = fopen(output_file, "w");
    if (!f)
    {
        perror("Error opening output file");
        return;
    }

    fprintf(f, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");

    const char *sensor_names[] = {
        "temperatura", "umidade", "luminosidade",
        "ruido", "eco2", "etvoc"};

    for (int i = 0; i < table->size; i++)
    {
        const DeviceStats *entry = &table->entries[i];
        for (int s = 0; s < SENSOR_COUNT; s++)
        {
            if (entry->sensors[s].count > 0)
            {
                double avg = entry->sensors[s].sum / entry->sensors[s].count;
                fprintf(f, "%s;%04d-%02d;%s;%.2f;%.2f;%.2f\n",
                        entry->device, entry->year, entry->month,
                        sensor_names[s],
                        entry->sensors[s].max,
                        avg,
                        entry->sensors[s].min);
            }
        }
    }

    fclose(f);
}

int main(int argc, char **argv)
{
    const char *input_file = (argc > 1) ? argv[1] : "devices.csv";
    const char *output_file = "output/results.csv";

    if (system("mkdir -p output") != 0)
    {
        perror("Error creating output directory");
        return 1;
    }

    StatsTable table;
    init_stats_table(&table, 1000);

    FILE *f = fopen(input_file, "r");
    if (!f)
    {
        perror("Error opening input file");
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    int total_lines = 0;
    int valid_lines = 0;

    if (!fgets(line, sizeof(line), f))
    {
        fclose(f);
        return 1;
    }

    while (fgets(line, sizeof(line), f))
    {
        total_lines++;
        if (process_line(line, &table))
        {
            valid_lines++;
        }
    }

    fclose(f);

    write_results(&table, output_file);

    free(table.entries);

    printf("Processing complete:\n");
    printf("Total lines processed: %d\n", total_lines);
    printf("Valid lines processed: %d\n", valid_lines);
    printf("Results written to %s\n", output_file);
    return 0;
}
