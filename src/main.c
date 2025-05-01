#include "csv_processor.h"
#include <stdio.h>

int main(void)
{
    size_t line_count = 0;
    char **lines = load_csv("devices.csv", &line_count);

    StatsTable table = process_csv_mt(lines, line_count);

    write_results(&table, "output");

    printf("Processamento concluído. Arquivo gerado em output/results.csv\n");

    free_csv_lines(lines, line_count);
    free_table(&table);
    return 0;
}
