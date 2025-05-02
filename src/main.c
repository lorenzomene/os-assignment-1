#include "csv_processor.h"
#include <locale.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    setlocale(LC_NUMERIC, "C");          /* evita bug de locale no strtod */

    const char *csv = (argc > 1) ? argv[1] : "devices.csv";

    size_t nlines = 0;
    char **lines = load_csv(csv, &nlines);

    StatsTable tbl = process_csv_mt(lines, nlines);
    write_results(&tbl, "output");

    printf("✓ Processamento concluído: output/results.csv\n");

    free_csv_lines(lines, nlines);
    free_table(&tbl);
    return 0;
}
