#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "0.2"

#define APPS_DIR "/apps/libreshop"

#define OVERSCAN_X 3
#define OVERSCAN_X_SPACES "   "

#define OVERSCAN_Y 2
#define OVERSCAN_Y_LINES_MINUS_1 "\n"

#define LINE "============================================================================="
#define BLNK "                                                                             "

#define PGLC 77 - OVERSCAN_X - 1, (OVERSCAN_X_SPACES "|                                                                           "), ("|" OVERSCAN_X_SPACES)
#define PGLN 77 - OVERSCAN_X - 1, (OVERSCAN_X_SPACES "+---------------------------------------------------------------------------"), ("+" OVERSCAN_X_SPACES)

#define PGLB 77 - OVERSCAN_X - 4, (OVERSCAN_X_SPACES "  (1/3) Downloading...                                                      "), ("0%  " OVERSCAN_X_SPACES)
#define PGEX 77 - OVERSCAN_X - 4, (OVERSCAN_X_SPACES "  (2/3) Extracting...                                                       "), ("0%  " OVERSCAN_X_SPACES)
#define PGIN 77 - OVERSCAN_X - 4, (OVERSCAN_X_SPACES "  Initializing extraction...                                                "), ("0%  " OVERSCAN_X_SPACES)

// do not change!
#define OVERSCAN_Y_LINES OVERSCAN_Y_LINES_MINUS_1 "\n"
#define OVERSCAN_Y_TIMES_2 (OVERSCAN_Y * 2)
#define OVERSCAN_X_WRAP_TO (77 - (OVERSCAN_X * 2))
#define OVERSCAN_X_PROGRESS (OVERSCAN_X_WRAP_TO - 4)

#endif
