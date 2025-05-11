// csv_parser.c
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE   2048
#define MAX_FIELDS 12

int main(int argc, char **argv) {
    const char *path = (argc > 1 ? argv[1] : "devices.csv");
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return EXIT_FAILURE; }

    char buf[MAX_LINE];
    // skip header
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return 0; }

    while (fgets(buf, sizeof buf, f)) {
        char *fields[MAX_FIELDS];
        int   nf = 0;
        char *line = buf;

        // split on '|' and also strip trailing '\n'
        while (nf < MAX_FIELDS && (fields[nf] = strsep(&line, "|\n")))
            nf++;

        if (nf != MAX_FIELDS) {
            // malformed or incomplete line
            continue;
        }

        // parse all columns
        int    id        = atoi(fields[0]);
        char  *device    = fields[1];
        int    contagem  = atoi(fields[2]);
        char  *datetime  = fields[3];
        double temperatura = atof(fields[4]);
        double umidade     = atof(fields[5]);
        double luminosidade= atof(fields[6]);
        double ruido       = atof(fields[7]);
        double eco2        = atof(fields[8]);
        double etvoc       = atof(fields[9]);
        double latitude    = atof(fields[10]);
        double longitude   = atof(fields[11]);

        // just print to verify
        printf(
            "id=%d dev=%s cnt=%d date=\"%s\" "
            "T=%.2f H=%.2f L=%.2f R=%.2f ECO2=%.2f eTVOC=%.2f "
            "lat=%.6f lon=%.6f\n",
            id, device, contagem, datetime,
            temperatura, umidade, luminosidade,
            ruido, eco2, etvoc, latitude, longitude
        );
    }

    fclose(f);
    return EXIT_SUCCESS;
}