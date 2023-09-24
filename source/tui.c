#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <wiiuse/wpad.h>
#include <time.h>
#include <math.h>

// needed for MIN
#include <sys/param.h>

#include <jansson.h>
#include <yuarel.h>
#include <zip.h>

#include <winyl/winyl.h>
#include <winyl/request.h>

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
    int endOfIndex = *(int*)offset + canDisplayAmount;

    printf("\x1b[27;20H");
    printf("   l %d o %d i %d cDA %d mO %d eOI %d   ", limit, *(int*)offset, *(int*)index, canDisplayAmount, maxOffset, endOfIndex);

    int ret = 0;
    while(true) {
        WPAD_ScanPads();
        
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_DOWN) {
            (*index)++;
            if (*(int*)index >= endOfIndex) {
                (*offset)++;
                if (*(int*)offset > maxOffset) (*offset)--;
            }
            if (*(int*)index >= limit) (*index)--;
            break;
        }
        else if (pressed & WPAD_BUTTON_UP) {
            (*index)--;
            if (*(int*)index < 0) {
                (*index)++;
            }
            if (*(int*)index < *(int*)offset) {
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

void progress_bar(int percent, int line) {
    int bars = floorf(percent * 0.71);
    printf("\x1b[%d;3H", line);
    char buf[72];
    for (int i = 0; i < 71; i++) {
        if (i < bars) buf[i] = '#';
        else buf[i] = ' ';
    }
    buf[71] = '\0';
    printf("%s", buf);

    printf("\x1b[2B\x1b[4D");
    for (int i = 0; i < (3 - intlen(percent)); i++) {
        printf(" ");
    }
    printf("%d", percent);
}

void download_app(const char* appname, const char* _hostname, json_t* app, char* user_agent) {
    clear_screen();

    json_t* urls = json_object_get(app, "url");
    json_t* sizes = json_object_get(app, "file_size");

    char* title = malloc(12 + strlen(appname) + 1);
    sprintf(title, "Downloading %s", appname);
    char* hostname = strdup(_hostname);

    clear_screen();
    print_topbar(title);

    for (int i = 0; i < 25; i++) {
        if (i == 11 || i == 13) printf(PGLN);
        else if (i == 12) printf(PGLC);
        else if (i == 14) printf("%s", PGLB);
        else printf("\n");
    }

    print_bottombar(0, 0, 24, "Please wait until the bars finish.");
    // clean out the "showing 1-0 of 0" text
    printf("\x1b[27;1H                ");

    char* fullurl = strdup(json_string_value(json_object_get(urls, "zip")));
    struct yuarel url_info;
    yuarel_parse(&url_info, fullurl);

    winyl w_dl = winyl_open(url_info.host, url_info.port == 0 ? 80 : url_info.port);
    winyl_response w_dl_res = winyl_request(&w_dl, url_info.path, WINYL_REQUEST_SLASH | WINYL_REQUEST_GET_SOCKET);
    free(fullurl);

    FILE* fp = fopen(APPS_DIR "/temp.zip", "w+");
    char buf[512];
    int received = 0;
    int current = 0;
    float total = json_integer_value(json_object_get(sizes, "zip_compressed")) / 100;
    while ((received = net_recv(w_dl_res.socket, buf, 512, 0)) > 0) {
        current += received;
        int percent = current / total;
        
        progress_bar(percent, 13);

        fwrite(buf, 1, received, fp);
    }
    fseek(fp, 0, SEEK_SET);

    winyl_response_close(&w_dl_res);
    winyl_close(&w_dl);

    /*
    printf("\x1b[1;0H");
    for (int i = 0; i < 25; i++) {
        printf("%d\n", i);
    }
    */

    printf("\x1b[9;0H");
    for (int i = 0; i < 9; i++) {
        if (i == 0 || i == 2 || i == 5 || i == 7) printf(PGLN);
        else if (i == 1 || i == 6) printf(PGLC);
        else if (i == 3) printf("%s", PGIN);
        else if (i == 4) printf(BLNK);
        else if (i == 8) printf("%s", PGEX);
    }
    
    zip_error_t error;
    zip_t* zip = zip_open_from_source(zip_source_filep_create(fp, 0, received, &error), 0, &error);

    zip_file_t* file;
    zip_stat_t stat;
    int entryamount = zip_get_num_entries(zip, 0);
    for (int i = 0; i < entryamount; i++) {
        zip_stat_index(zip, i, 0, &stat);

        int slashes = 0;
        for (int j = 0; stat.name[j] != '\0'; j++) {
            if (stat.name[j] == '/') slashes++;
        }

        char* name = strdup(stat.name);
        name[0] = '/';

        int slashes2 = 0;
        DIR* check;
        for (int j = 0; stat.name[j] != '\0'; j++) {
            if (stat.name[j] == '/') {
                slashes2++;
                if (slashes == slashes2) {
                    for (int k = 0; k <= j; k++) {
                        name[k + 1] = stat.name[k];
                        if (stat.name[k] == '/') {
                            name[k + 1] = '\0';
                            
                            check = opendir(name);
                            if (!check) {
                                if (mkdir(name, 0600) != 0) {
                                    printf("\ncreating directory %s failed - home to exit\n", name);
                                    home_exit(true);
                                }
                            }
                            else closedir(check);
                            
                            name[k + 1] = '/';
                        }
                    }
                }
            }
        }

        free(name);

        char* filename = malloc(strlen(stat.name) + 2);
        sprintf(filename, "/%s", stat.name);

        printf("\x1b[12;0H" BLNP "\x1b[3C%%\x1b[%dD (%d/%d) Extracting file %s", 3 + strlen(BLNP), i + 1, entryamount, filename);

        FILE* fp = fopen(filename, "wb");
        if (fp == NULL) {
            printf("\nCannot open %s.\n", filename);
            home_exit(true);
        }

        int bufsize = 1024;
        char buf[bufsize];
        
        current = 0;
        float total = stat.size;
        
        file = zip_fopen_index(zip, i, 0);
        while ((received = zip_fread(file, buf, bufsize)) > 0) {
            current += received;

            int percent = (current / total) * 100;
            progress_bar(percent, 10);
            fwrite(buf, received, 1, fp);
        }
        zip_fclose(file);

        float index1 = i + 1;
        int percent = (index1 / entryamount) * 100;
        progress_bar(percent, 15);

        free(filename);
    }
    
    zip_close(zip);

    printf("\x1b[17;2H(3/3) Installation complete.");
    printf("\x1b[27;42H     Press any button to continue.");
    
    while(true) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0)) break;
    }

    free(title);
    free(hostname);
}

void app_info(json_t* config, const char* hostname, json_t* app, char* user_agent) {
    int zero0 = 0;
    int zero1 = 0;
    
    const char* name = json_string_value(json_object_get(app, "name"));
    const char* version = json_string_value(json_object_get(app, "version"));
    const char* authors = json_string_value(json_object_get(app, "author"));
    time_t release_ts = json_integer_value(json_object_get(app, "release_date"));

    char timestring[12];
    strftime(timestring, 12, "%d %b %Y", gmtime(&release_ts));

    const char* description = json_string_value(json_object_get(json_object_get(app, "description"), "long"));

    char* title = malloc(strlen(name) + 1);
    sprintf(title, "%s", name);

    while(true) {
        clear_screen();
        print_topbar(title);

        printf("\n");
        printf(" App name: %s\n", name);
        printf(" App version: %s\n", version);
        printf(" Created by %s\n", authors);
        printf(" Released on %s\n", timestring);
        printf("\n");
        printf("%s\n", description);
        printf("\n");

        printf("\n");

        print_bottombar(1, 0, 24, "A to download, B for back, HOME to exit");

        int ret = process_inputs(0, &zero0, &zero1);
        if (ret == 2) break;
        else if (ret == -1) {
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
        else if (ret == 1) {
            download_app(name, hostname, app, user_agent);
        }
    }

    free(title);
}

void surf_category(json_t* config, const char* hostname, const char* name, json_t* category, char* user_agent) {
    const char* categoryname = json_string_value(json_object_get(category, "display_name"));
    const char* catslug = json_string_value(json_object_get(category, "name"));

    json_t* repos = json_object_get(config, "repos");
    json_t* repo = json_object_get(repos, hostname);
    json_t* contents = json_object_get(repo, "contents");

    json_t* apps = json_object_get(contents, catslug);
    int appsamount = json_array_size(apps);
    
    char* title = malloc(8 + strlen(name) + 3 + strlen(categoryname) + 1);
    sprintf(title, "Surfing %s > %s", name, categoryname);

    int index = 0;
    int offset = 0;
    while(true) {
        clear_screen();
        print_topbar(title);

        int printable = MIN(25, appsamount);
        for (int i = offset; i < (offset + printable); i++) {
            json_t* app = json_array_get(apps, i);
            
            print_cursor(i, index);
            printf("%s\n", json_string_value(json_object_get(app, "name")));
        }

        print_bottombar(appsamount, offset, printable - 1, "A to engage, B for back, HOME to exit");
        int ret = process_inputs(appsamount, &offset, &index);
        if (ret == 2) break;
        else if (ret == -1) {
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
        else if (ret == 1) {
            app_info(config, hostname, json_array_get(apps, index), user_agent);
        }
    }

    free(title);
}

void surf_repository(json_t* config, const char* hostname, char* user_agent) {
    json_t* repos = json_object_get(config, "repos");
    json_t* repo = json_object_get(repos, hostname);
    json_t* information = json_object_get(repo, "information");

    json_t* categories = json_object_get(information, "available_categories");
    int arraysize = json_array_size(categories);

    const char* name = json_string_value(json_object_get(information, "name"));

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
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
        else if (ret == 1) {
            surf_category(config, hostname, name, json_array_get(categories, index), user_agent);
        }
    }
    free(title);
}

void start_tui(json_t* config, char* user_agent) {
    json_t* repositories = json_object_get(config, "repositories");
    int reposamount = json_array_size(repositories);
    json_t* repos = json_object_get(config, "repos");

    int index = 0;
    int offset = 0;
    for (int i = 0; true; i++) {
        if (i != 0 || reposamount != 1) {
            clear_screen();
            print_topbar("Pick a repository");
    
            int printable = MIN(25, reposamount);
            for (int j = offset; j < (offset + printable); j++) {
                json_t* repo = json_object_get(repos, json_string_value(json_array_get(repositories, j)));
                json_t* information = json_object_get(repo, "information");

                const char* provider = json_string_value(json_object_get(information, "provider"));
                const char* name = json_string_value(json_object_get(information, "name"));

                print_cursor(j, index);
                printf("%s: %s\n", provider, name);
            }

            print_bottombar(reposamount, offset, printable - 1, "A to engage, 1 for settings, HOME to exit");
        }
        int ret = (i == 0 && reposamount == 1) ? 1 : process_inputs(reposamount, &offset, &index);

        if (ret == -1) break;
        else if (ret == 1) {
            const char* hostname = json_string_value(json_array_get(repositories, index));
            surf_repository(config, hostname, user_agent);
        }
        else if (ret == 3) {
            clear_screen();
            printf("settings here\n");
            debug_npause();
        }
    }
}
