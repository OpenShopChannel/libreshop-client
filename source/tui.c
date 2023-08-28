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
        else if (pressed & WPAD_BUTTON_B) {
            ret = 2;
            break;
        }
        else if (pressed & WPAD_BUTTON_1) {
            ret = 3;
            break;
        }
    }

    return ret;
}

void print_cursor(int currentfile, int cursor) {
    if (currentfile == cursor) {
        printf(" > ");
    }
    else printf("   ");
}

int intlen(int n) {
    int place = 1;
    while (n > 9) {
        n /= 10;
        place++;
    }
    return place;
}

void print_bottombar(int limit, int offset, int file, char* bottom) {
    for (int i = file; i < 24; i++) {
        printf("\n");
    }
    int start = offset + 1;
    int end = offset + MIN(25, limit);
   
    int linelen = 9 + intlen(start) + 1 + intlen(end) + 4 + intlen(limit) + 1;
    char* wholeline = malloc(linelen);
    sprintf(wholeline, " showing %d-%d of %d", start, end, limit);

    int bottomlen = strlen(bottom);
    int spaces = 77 - (linelen - 1) - bottomlen - 1;
    char buf[spaces + 1];
    buf[spaces] = '\0';
    for (int i = 0; i < spaces; i++) {
        buf[i] = ' ';
    }

    printf(LINE "%s%s%s", wholeline, buf, bottom);
    
    free(wholeline);
}

void surf_repository(json_t* config, const char* hostname) {
    json_error_t error;

    char* file = malloc(strlen(REPO_DIR) + 1 + 2 + strlen(hostname) + 5 + 1); // plus slash plus I_ plus .json plus nul
    sprintf(file, "%s/I_%s.json", REPO_DIR, hostname);

    json_t* repo = json_load_file(file, 0, &error);
    if (!repo) {
        printf("Failed reading JSON %s.\n", file);
        free(file);
        home_exit(true);
    }

    json_t* categories = json_object_get(repo, "available_categories");
    int arraysize = json_array_size(categories);

    const char* name = json_string_value(json_object_get(repo, "name"));

    char* title = malloc(strlen(name) + 1 + 8);
    sprintf(title, "Surfing %s", name);

    int index = 0;
    int offset = 0;
    while(true) {
        clear_screen();
        print_topbar(title);

        int processed = MIN(25, arraysize);
        for (int i = offset; i < (offset + processed); i++) {
            json_t* category = json_array_get(categories, i);

            print_cursor(i, index);
            printf("%s\n", json_string_value(json_object_get(category, "display_name")));
        }

        print_bottombar(arraysize, offset, processed - 1, "A to engage, B for back, HOME to exit");
        int ret = process_inputs(arraysize, &offset, &index);
        if (ret == 2) break;
        else if (ret == -1) {
            free(file);
            free(title);
            home_exit(true);
        }
        else if (ret == 1) {
            clear_screen();
            printf("apps here\n");
            debug_npause();
        }
    }

    free(file);
    free(title);
}

void start_tui(json_t* config) {
    json_error_t error;
    
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
            if (item->d_name[0] == 'D') continue;
            if (skipped > 0) {
                skipped--;
                continue;
            }
            file++;
            if (file >= 25) break;

            char* filepath = malloc(strlen(REPO_DIR) + 1 + strlen(item->d_name) + 1); // plus slash plus nul
            sprintf(filepath, "%s/%s", REPO_DIR, item->d_name);
            
            json_t* repo = json_load_file(filepath, 0, &error);
            const char* provider = json_string_value(json_object_get(repo, "provider"));
            const char* name = json_string_value(json_object_get(repo, "name"));

            print_cursor(file, index);
            printf("%s: %s\n", provider, name);

            json_decref(repo);
            free(filepath);
        }


        print_bottombar(reposamount, offset, file, "A to engage, 1 for settings, HOME to exit");
        int ret = process_inputs(reposamount, &offset, &index);

        if (ret == -1) break;
        else if (ret == 1) {
            const char* hostname = json_string_value(json_array_get(repositories, index));
            surf_repository(config, hostname);
        }
        else if (ret == 3) {
            clear_screen();
            printf("settings here\n");
            debug_npause();
        }
    }

    closedir(repos);
}
