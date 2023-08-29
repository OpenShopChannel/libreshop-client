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

void app_info(json_t* config, json_t* app) {
    clear_screen();
    printf("app name: %s\n", json_string_value(json_object_get(app, "name")));
    printf("by %s\n", json_string_value(json_object_get(app, "author")));
    debug_npause();
}

void surf_category(json_t* config, const char* hostname, const char* name, json_t* category) {
    json_error_t error;

    const char* categoryname = json_string_value(json_object_get(category, "display_name"));
    const char* catslug = json_string_value(json_object_get(category, "name"));

    char* directory = malloc(strlen(REPO_DIR) + 1 + 2 + strlen(hostname) + 1 + strlen(catslug) + 1);
    sprintf(directory, REPO_DIR "/D_%s/%s", hostname, catslug);

    char* jsonname = malloc(strlen(hostname) + 1 + strlen(catslug) + 1 + 2);
    sprintf(jsonname, "%s+%s", hostname, catslug);
    for (int i = 0; i < strlen(jsonname); i++) {
        if (jsonname[i] == '.') jsonname[i] = '_';
    }
    
    char* jsonname_arr = malloc(strlen(jsonname) + 3);
    sprintf(jsonname_arr, "D_%s", jsonname);

    json_t* widetemp = json_object_get(config, "temp");
    json_t* temp = json_object_get(widetemp, jsonname);

    int appsamount = 0;
    if (!temp) {
        json_object_set(widetemp, jsonname, json_object());
        temp = json_object_get(widetemp, jsonname);
        json_object_set(widetemp, jsonname_arr, json_array());
        json_t* temptemp_arr = json_object_get(widetemp, jsonname_arr);

        DIR* dir = opendir(directory);
        if (dir == NULL) {
            printf("Could not open %s.\n", directory);
            free(directory);
            free(jsonname);
            free(jsonname_arr);
            home_exit(true);
        }
        struct dirent* item;

        while ((item = readdir(dir)) != NULL) {
            if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
            int itemlen = strlen(directory) + 1 + strlen(item->d_name);
            char* itempath = malloc(itemlen + 1);
            sprintf(itempath, "%s/%s", directory, item->d_name);

            json_t* app = json_load_file(itempath, 0, &error);
            if (!app) {
                free(itempath);
                free(directory);
                free(jsonname);
                free(jsonname_arr);
                home_exit(true);
            }

            const char* appname = json_string_value(json_object_get(app, "name"));
            json_object_set(temp, appname, app);
            json_array_append(temptemp_arr, json_string(appname));

            appsamount++;
            free(itempath);
        }

        closedir(dir);
    }
    else {
        appsamount = json_array_size(json_object_get(widetemp, jsonname_arr));
    }
    json_t* temp_arr = json_object_get(widetemp, jsonname_arr);

    char* title = malloc(8 + strlen(name) + 3 + strlen(categoryname) + 1);
    sprintf(title, "Surfing %s > %s", name, categoryname);

    int index = 0;
    int offset = 0;
    while(true) {
        clear_screen();
        print_topbar(title);

        int printable = MIN(25, appsamount);
        for (int i = 0; i < (offset + printable); i++) {
            json_t* item = json_array_get(temp_arr, i);
            if (i < offset) continue;
            i -= offset;
            print_cursor(i, index);
            printf("%s\n", json_string_value(item));
            i += offset;
        }

        print_bottombar(appsamount, offset, printable - 1, "A to engage, B for back, HOME to exit");
        int ret = process_inputs(appsamount, &offset, &index);
        if (ret == 2) break;
        else if (ret == -1) {
            free(title);
            free(directory);
            free(jsonname);
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
        else if (ret == 1) {
            app_info(config, json_object_get(temp, json_string_value(json_array_get(temp_arr, index))));
        }
    }

    free(title);
    free(directory);
    free(jsonname);
    free(jsonname_arr);
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
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
        else if (ret == 1) {
            surf_category(config, hostname, name, json_array_get(categories, index));
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
    for (int i = 0; true; i++) {
        if (i != 0 || reposamount != 1) {
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
        }
        int ret = (i == 0 && reposamount == 1) ? 1 : process_inputs(reposamount, &offset, &index);

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
