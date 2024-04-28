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

#include "acknowledgements_json.h"

void clear_screen() {
    printf("\x1b[2J");
}

void print_topbar(char* msg) {
    int spacesneeded = 77 - OVERSCAN_X * 2 -     11    -   2  - strlen(msg) - strlen(VERSION);
    /*                                      LibreShop_v spaces */
    char buf[spacesneeded + 1];
    for (int i = 0; i < spacesneeded; i++) {
        buf[i] = ' ';
    }
    buf[spacesneeded] = '\0';
    printf(OVERSCAN_Y_LINES OVERSCAN_X_SPACES "%s %s LibreShop v" VERSION "\n" LINE, msg, buf);
}

// these pointer stuff are so confusing. damn im a javascript dev do yall think i need this in my life
int process_inputs(int limit, int* offset, int* index, int noscroll) {
    int canDisplayAmount = MIN(25 - OVERSCAN_Y_TIMES_2, limit - *(int*)offset);
    int maxOffset = MAX(0, limit - canDisplayAmount);
    int endOfIndex = *(int*)offset + canDisplayAmount;

    //printf("\x1b[27;20H");
    //printf("   l %d o %d i %d cDA %d mO %d eOI %d   ", limit, *(int*)offset, *(int*)index, canDisplayAmount, maxOffset, endOfIndex);

    int ret = 0;
    while(true) {
        WPAD_ScanPads();
        
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_DOWN || (noscroll && (pressed & WPAD_BUTTON_RIGHT))) {
            (*index)++;
            if (noscroll) (*index) = endOfIndex;
            if (*(int*)index >= endOfIndex) {
                (*offset)++;
                if (*(int*)offset > maxOffset) (*offset)--;
            }
            if (*(int*)index >= limit) (*index)--;
            ret = 4;
            break;
        }
        else if (pressed & WPAD_BUTTON_UP || (noscroll && (pressed & WPAD_BUTTON_LEFT))) {
            (*index)--;
            if (noscroll) (*index) = *(int*)offset - 1;
            if (*(int*)index < *(int*)offset) {
                (*offset)--;
                if (*(int*)offset < 0) (*offset)++;
            }
            if (*(int*)index < 0) (*index)++;
            ret = 5;
            break;
        }
        else if (!noscroll && pressed & WPAD_BUTTON_LEFT) {
            (*index) -= canDisplayAmount;
            (*offset) -= canDisplayAmount;
            if (*(int*)index < 0) (*index) = 0;
            if (*(int*)offset < 0) (*offset) = 0;
            break;
        }
        else if (!noscroll && pressed & WPAD_BUTTON_RIGHT) {
            (*index) += canDisplayAmount;
            (*offset) += canDisplayAmount;
            if (*(int*)index >= limit) (*index) = limit - 1;
            if (*(int*)offset > maxOffset) (*offset) = maxOffset;
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
    char cur[OVERSCAN_X + 1];
    strncpy(cur, OVERSCAN_X_SPACES, OVERSCAN_X + 1);
    if (currentfile == cursor) cur[OVERSCAN_X - 2] = '>';
    printf("%s", cur);
}

void print_bottombar(int limit, int offset, int file, char* bottom, int noscroll) {
    for (int i = file; i < (24 - OVERSCAN_Y_TIMES_2); i++) {
        printf("\n");
    }
    int start = offset + 1;
    int end = offset + MIN(25 - OVERSCAN_Y_TIMES_2, limit);
   
    int linelen =    8   + WINYL_INTLEN(start) + 1 + WINYL_INTLEN(end) + 4  + WINYL_INTLEN(limit) +  1 ;
    /*            showing                        -                       of                         nul */
    if (noscroll) linelen = 1;
    char* wholeline = malloc(linelen);
    if (!noscroll) sprintf(wholeline, "showing %d-%d of %d", start, end, limit);
    else wholeline[0] = '\0';

    int bottomlen = strlen(bottom);
    int spaces = 77 - OVERSCAN_X * 2 - (linelen - 1) - bottomlen;

    char buf[spaces + 1];
    buf[spaces] = '\0';
    for (int i = 0; i < spaces; i++) {
        buf[i] = ' ';
    }

    printf(LINE OVERSCAN_X_SPACES "%s%s%s", wholeline, buf, bottom);
    
    free(wholeline);
}

void progress_bar(int percent, int line) {
    float total = OVERSCAN_X_PROGRESS / 100.0f;
    int bars = percent * total;

    printf("\x1b[%d;%dH", line, OVERSCAN_X + 2);

    char buf[OVERSCAN_X_PROGRESS + 1];
    for (int i = 0; i < OVERSCAN_X_PROGRESS; i++) {
        if (i < bars) buf[i] = '#';
        else buf[i] = ' ';
    }
    buf[OVERSCAN_X_PROGRESS] = '\0';
    printf("%s", buf);

    printf("\x1b[2B\x1b[4D");
    for (int i = 0; i < (3 - WINYL_INTLEN(percent)); i++) {
        printf(" ");
    }
    printf("%d", percent);
}

void download_app(const char* appname, const char* _hostname, json_t* app, char* user_agent) {
    json_t* urls = json_object_get(app, "url");
    json_t* sizes = json_object_get(app, "file_size");

    char* title = malloc(12 + strlen(appname) + 1);
    sprintf(title, "Downloading %s", appname);
    char* hostname = strdup(_hostname);

    clear_screen();
    print_topbar(title);

    int center = (25 - OVERSCAN_Y_TIMES_2) / 2;
    for (int i = 0; i < (25 - OVERSCAN_Y_TIMES_2); i++) {
        if (i == (center - 1) || i == (center + 1)) printf("%.*s%s", PGLN);
        else if (i == center) printf("%.*s%s", PGLC);
        else if (i == (center + 2)) printf("%.*s%s", PGLB);
        else printf("\n");
    }

    print_bottombar(0, 0, 24, "Please wait until the bars finish.", 1);

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
        
        progress_bar(percent, 14);

        fwrite(buf, 1, received, fp);
    }
    fseek(fp, 0, SEEK_SET);

    winyl_response_close(&w_dl_res);
    winyl_close(&w_dl);

    printf("\x1b[9;0H");
    for (int i = 0; i < 9; i++) {
        if (i == 0 || i == 2 || i == 5 || i == 7) printf("%.*s%s", PGLN);
        else if (i == 1 || i == 6) printf("%.*s%s", PGLC);
        else if (i == 3) printf("%.*s%s", PGIN);
        else if (i == 4) printf(BLNK);
        else if (i == 8) printf("%.*s%s", PGEX);
    }
    
    zip_error_t error;
    zip_t* zip = zip_open_from_source(zip_source_filep_create(fp, 0, received, &error), 0, &error);

    zip_file_t* file;
    zip_stat_t stat;
    int entryamount = zip_get_num_entries(zip, 0);
    int percent = 0;
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

        printf("\x1b[12;0H" BLNK "\x1b[1A" OVERSCAN_X_SPACES "  (%d/%d) Extracting file %s\x1b[12;%dH%%", i + 1, entryamount, filename, 77 - OVERSCAN_X - 3);

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
        percent = (index1 / entryamount) * 100;
        progress_bar(percent, 15);

        free(filename);
    }
    
    zip_close(zip);

    printf("\x1b[17;%dH(3/3) Installation complete.", OVERSCAN_X + 2);
    printf("\x1b[%d;%dH     Press any button to continue.", 28 - OVERSCAN_Y, 43 - OVERSCAN_X);

    while(true) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0)) break;
    }

    free(title);
    free(hostname);
}

void app_info(json_t* config, const char* hostname, json_t* app, char* user_agent) {
    char* name = strdup(json_string_value(json_object_get(app, "name")));
    const char* version = json_string_value(json_object_get(app, "version"));
    const char* authors = json_string_value(json_object_get(app, "author"));
    time_t release_ts = json_integer_value(json_object_get(app, "release_date"));

    char timestring[12];
    strftime(timestring, 12, "%d %b %Y", gmtime(&release_ts));

    FILE* temp = fopen(APPS_DIR "/temp.txt", "w+");

    int i = 77, j, lines = 0;
    
    fprintf(temp, OVERSCAN_X_SPACES "App name: %s", name);
    for (j = ftell(temp); j < i; j++) fputc(' ', temp);
    lines++, i += 77;

    fprintf(temp, OVERSCAN_X_SPACES "App version: %s", version);
    for (j = ftell(temp); j < i; j++) fputc(' ', temp);
    lines++, i += 77;

    fprintf(temp, OVERSCAN_X_SPACES "Created by %s", authors);
    for (j = ftell(temp); j < i; j++) fputc(' ', temp);
    lines++, i += 77 * 2;
    
    fprintf(temp, OVERSCAN_X_SPACES "Released on %s", timestring);
    for (j = ftell(temp); j < i; j++) fputc(' ', temp);
    lines += 2;

    const char* description = json_string_value(json_object_get(json_object_get(app, "description"), "long"));
    if (description) {
        int desclen = strlen(description);

        for (i = 0; i < desclen; j = 0) {
            fprintf(temp, OVERSCAN_X_SPACES);

            for (j = i; j < (i + OVERSCAN_X_WRAP_TO) && j < desclen && description[j] != '\n'; j++) {
                if (description[j] != '\t') fputc(description[j], temp);
                else fputc(' ', temp);
            }

            int backupI = i;
            i = j + (description[j] == '\n');

            for ((void)j; j < (backupI + OVERSCAN_X_WRAP_TO); j++) {
                fputc(' ', temp);
            }

            fprintf(temp, OVERSCAN_X_SPACES);
            lines++;
        }
    }

    int index = 0;
    int offset = 0;
    char line[78];
    line[77] = '\0';
    while(true) {
        clear_screen();
        print_topbar(name);

        fseek(temp, offset * 77, SEEK_SET);

        int printable = MIN(25 - OVERSCAN_Y_TIMES_2, lines);
        for (int k = offset; k < (offset + printable); k++) {
            fread(line, 77, 1, temp);
            printf("%s", line);
        }

        print_bottombar(lines, offset, printable - 1, "A to download, B for back, HOME to exit", 0);

        int ret = process_inputs(lines, &offset, &index, 1);
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

    free(name);
    fclose(temp);
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

        int printable = MIN(25 - OVERSCAN_Y_TIMES_2, appsamount);
        for (int i = offset; i < (offset + printable); i++) {
            json_t* app = json_array_get(apps, i);
            
            print_cursor(i, index);
            printf("%s\n", json_string_value(json_object_get(app, "name")));
        }

        print_bottombar(appsamount, offset, printable - 1, "A to engage, B for back, HOME to exit", 0);
        int ret = process_inputs(appsamount, &offset, &index, 0);
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

        int processed = MIN(25 - OVERSCAN_Y_TIMES_2, arraysize);
        for (int i = offset; i < (offset + processed); i++) {
            json_t* category = json_array_get(categories, i);

            print_cursor(i, index);
            printf("%s\n", json_string_value(json_object_get(category, "display_name")));
        }

        print_bottombar(arraysize, offset, processed - 1, "A to engage, B for back, HOME to exit", 0);
        int ret = process_inputs(arraysize, &offset, &index, 0);
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

void open_settings() {
    int index = 0;
    int offset = 0;

    int lines = acknowledgements_json_size / 77;
    char buf[78];
    buf[77] = '\0';

    while(true) {
        clear_screen();
        print_topbar("Settings");

        int printable = MIN(25 - OVERSCAN_Y_TIMES_2, lines);
        for (int k = offset; k < (offset + printable); k++) {
            for (int l = 0; l < 77; l++) {
                buf[l] = acknowledgements_json[l + (k * 77)];
            }
            printf("%s", buf);
        }

        print_bottombar(lines, offset, printable - 1, "B for back, HOME to exit", 0);

        int ret = process_inputs(lines, &offset, &index, 1);
        if (ret == 2) break;
        else if (ret == -1) {
            clear_screen();
            printf("Exiting...\n");
            exit(0);
        }
    }
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
    
            int printable = MIN(25 - OVERSCAN_Y_TIMES_2, reposamount);
            for (int j = offset; j < (offset + printable); j++) {
                json_t* repo = json_object_get(repos, json_string_value(json_array_get(repositories, j)));
                json_t* information = json_object_get(repo, "information");

                const char* provider = json_string_value(json_object_get(information, "provider"));
                const char* name = json_string_value(json_object_get(information, "name"));

                print_cursor(j, index);
                printf("%s: %s\n", provider, name);
            }

            print_bottombar(reposamount, offset, printable - 1, "A to engage, 1 for settings, HOME to exit", 0);
        }
        int ret = (i == 0 && reposamount == 1) ? 1 : process_inputs(reposamount, &offset, &index, 0);

        if (ret == -1) break;
        else if (ret == 1) {
            const char* hostname = json_string_value(json_array_get(repositories, index));
            surf_repository(config, hostname, user_agent);
        }
        else if (ret == 3) {
            open_settings();
        }
    }
}
