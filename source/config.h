#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "0.0dev"

#define APPS_DIR "/apps/libreshop"

#define OVERSCAN_X 6
#define OVERSCAN_X_SPACES "      "

#define OVERSCAN_Y 3
#define OVERSCAN_Y_LINES_MINUS_1 "\n\n"

#define LINE "============================================================================="
#define PGLN " +-------------------------------------------------------------------------+ "
#define PGLC " |                                                                         | "
#define PGLB "  (1/3) Downloading...                                                  0%   "
#define PGEX "  (2/3) Extracting...                                                   0%   "
#define PGIN "  Initializing extraction...                                            0%   "
#define BLNK "                                                                             "
#define BLNP "                                                                      "

// do not change!
#define OVERSCAN_Y_LINES OVERSCAN_Y_LINES_MINUS_1 "\n"
#define OVERSCAN_Y_TIMES_2 (OVERSCAN_Y * 2)
#define OVERSCAN_X_WRAP_TO (77 - (OVERSCAN_X * 2))

#endif
