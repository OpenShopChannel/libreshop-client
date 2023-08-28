#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <wiiuse/wpad.h>

// needed for MAX
#include <sys/param.h>

#include <jansson.h>

#include "debug.h"
#include "config.h"
#include "main.h"

void clear_screen() {
    printf("\x1b[2J");
}

void print_topbar(char* msg) {
    int spacesneeded = 77 - 11 - strlen(VERSION) - 4 - strlen(msg); // i have no idea how i landed on this but it works
    char buf[spacesneeded + 1];
    for (int i = 0; i < spacesneeded; i++) {
        buf[i] = ' ';
    }
    buf[spacesneeded] = '\0';
    printf(" %s %s LibreShop v" VERSION "\n" LINE, msg, buf);
}

// these pointer stuff are so confusing. damn im a javascript dev do yall think i need this in my life
int process_inputs(int limit, int* offset, int* index) {
    int canDisplayAmount = MIN(25, limit - *(int*)offset);
    int maxOffset = MAX(0, limit - canDisplayAmount);
    
    int ret = 0;
    while(true) {
        WPAD_ScanPads();
        
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_DOWN) {
            (*index)++;
            if (*(int*)index >= canDisplayAmount) {
                (*index)--;
                (*offset)++;
                if (*(int*)offset > maxOffset) (*offset)--;
            }
            break;
        }
        else if (pressed & WPAD_BUTTON_UP) {
            (*index)--;
            if (*(int*)index < 0) {
                (*index)++;
                (*offset)--;
                if (*(int*)offset < 0) (*offset)++;
            }
            break;
        }
        else if (pressed & WPAD_BUTTON_HOME) {
            ret = -1;
            break;
        }
        else if (pressed & WPAD_BUTTON_A) {
            ret = 1;
            break;
        }
    }

    return ret;
}

void print_cursor(int currentfile, int cursor) {
    if (currentfile == cursor) printf(" > ");
    else printf("   ");
}

void print_bottombar(int limit, int offset, int file) {
    for (int i = file; i < 24; i++) {
        printf("\n");
    }
    printf(LINE "showing %d-%d of %d", offset + 1, offset + MIN(25, limit), limit);
}

void surf_repository(json_t* config, const char* hostname) {
    char* title = malloc(strlen(hostname) + 1 + 8);
    sprintf(title, "Surfing %s", hostname);

    int zero0 = 0;
    int zero1 = 0;
    while(true) {
        clear_screen();
        print_topbar(title);

        print_cursor(0, 0);
        printf("Go back\n");

        print_bottombar(1, 0, 0);

        int ret = process_inputs(1, &zero0, &zero1);
        if (ret != 0) break;
    }

    free(title);
}

void start_tui(json_t* config) {
    DIR* repos = opendir(REPO_DIR);
    if (repos == NULL) {
        json_decref(config);
        printf("Could not open " REPO_DIR ".\n");
        home_exit(1);
    }
    struct dirent* item;

    json_t* repositories = json_object_get(config, "repositories");
    int reposamount = json_array_size(repositories);
    int index = 0;
    int offset = 0;
    while(true) {
        clear_screen();
        print_topbar("Pick a repository");
    
        rewinddir(repos);
        
        int file = -1;
        int skipped = offset;
        while ((item = readdir(repos)) != NULL) {
            if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
            if (skipped > 0) {
                skipped--;
                continue;
            }
            file++;
            if (file >= 25) break;
            
            print_cursor(file, index);
            printf("%s\n", item->d_name);
        }


        print_bottombar(reposamount, offset, file);
        int ret = process_inputs(reposamount, &offset, &index);

        if (ret == -1) break;
        else if (ret == 1) {
            const char* hostname = json_string_value(json_array_get(repositories, index));
            surf_repository(config, hostname);
        }
    }

    closedir(repos);
}
