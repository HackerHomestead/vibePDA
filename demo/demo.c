/* demo.c - Populate data directory with demo data (Parks and Rec themed) */

#include "config.h"
#include "../tests/fixture_parks.h"
#include "../src/vibe_config.h"
#include "../src/storage.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int n = 100;
    if (argc >= 2) {
        n = atoi(argv[1]);
        if (n < 1 || n > 10000) {
            fprintf(stderr, "Usage: %s [COUNT]\n", argv[0]);
            fprintf(stderr, "  COUNT: number of records (1-10000, default: 100)\n");
            return 1;
        }
    }

    VibeConfig cfg;
    vibe_config_load(&cfg);

    printf("Seeding %d records into: %s\n", n, cfg.data_dir);
    printf("(Parks and Rec themed)\n");

    fixture_seed_parks(cfg.data_dir, n);

    printf("Done! %d records created.\n", n);
    return 0;
}
