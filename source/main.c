#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include <sandia.h>
#include <jansson.h>

#include "libs/fshelp.h"
#include "libs/nethelp.h"

#include "debug.h"
#include "config.h"
#include "tui.h"

#include "default_config_json.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void printheader() {
	printf("\x1b[44m");
	printf("                                                                           \n");
	printf("     LibreShop v%s                                                     \n", VERSION);
	printf("                                                                           \n");
	printf("\x1b[40m\n");
}

void logprint(int type, char *message) {
	switch(type) {
		case 1:
			printf("\x1b[34m[\x1b[32mOK\x1b[34m]");
			break;
		case 2:
			printf("\x1b[34m[\x1b[31m--\x1b[34m]");
			break;
		case 3:
			printf("\x1b[31m!!\x1b[34m]");
			break;
		default:
			printf("    ");
	}
	printf(" \x1b[37m%s", message);
}

void home_exit(int message) {
    if (message) logprint(0, "Press HOME to exit\n");
    
    while(1) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        
        if (pressed & WPAD_BUTTON_HOME) {
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
	s32 ret;

	char localip[16] = {0};
	char gateway[16] = {0};
	char netmask[16] = {0};

	VIDEO_Init();

	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("\x1b[1;0H");

	printheader();
	logprint(0, "Welcome to LibreShop!\n");
#ifdef DEBUG_H
	logprint(2, "Warning! You are running a DEBUG build.\n");
#endif

        printf("\n");
        logprint(0, "Initializing network.\n");

	ret = if_config(localip, netmask, gateway, TRUE, 20);

	if (ret >= 0) {
		logprint(1, "Initialized successfully!\n");
                printf("\n");
		logprint(0, "Your Wii's IP address is ");
                printf("%s.\n", localip);
        }
        else {
		logprint(2, "Network configuration failed :(\n");
                home_exit(true);
        }

        printf("\n");
        logprint(0, "Mounting SD card.\n");
        if (fatInitDefault()) {
            logprint(1, "Mounted successfully!\n");
        }
        else {
            logprint(2, "SD card mounting failed :(\n");
            home_exit(true);
        }

        printf("\n");

        #ifndef SKIP_LIBRARY_BOOT
        DIR* library_check = opendir(REPO_DIR);
        if (!library_check) {
            closedir(library_check);
            logprint(0, "Creating library directory.\n");
        
            char dirs[3][strlen(REPO_DIR) + 1];
            strcpy(dirs[0], "/apps");
            strcpy(dirs[1], APPS_DIR);
            strcpy(dirs[2], REPO_DIR);

            for (int i = 0; i < 3; i++) {
                DIR* check = opendir(dirs[i]);
                if (!check) {
                    closedir(check);

                    if (mkdir(dirs[i], 0600) != 0) {
                        printf("Creating directory %s failed :(\n", dirs[i]);
                        home_exit(true);
                    }
                }
                else closedir(check);
            }
        }
        else {
            closedir(library_check);
            logprint(0, "Cleaning library directory.\n");
        
            if (delete_directory(REPO_DIR "/") != 0) {
                logprint(2, "Removing library directory failed :(\n");
                home_exit(true);
            }

            if (mkdir(REPO_DIR, 0600) != 0) {
                logprint(2, "Creating library directory failed :(\n");
                home_exit(true);
            }
        }
        logprint(1, "Done!\n");

        printf("\n");

        FILE* _config = fopen(APPS_DIR "/config.json", "r");
        #ifdef ALWAYS_DEFAULT_CONFIG
        fclose(_config);
        _config = NULL;
        #endif
        if (_config == NULL) {
            logprint(0, "Writing default configuration.\n");
            _config = fopen(APPS_DIR "/config.json", "w+");
            fwrite(default_config_json, default_config_json_size, 1, _config);
            fseek(_config, 0, SEEK_SET);
        }

        logprint(0, "Loading configuration.\n");
        json_error_t error;
        json_t* config = json_loadf(_config, 0, &error);
        fclose(_config);

        if (!config) {
            printf("Could not load configuration. JSON error at line %d: %s\n", error.line, error.text);
            home_exit(true);
        }
        else logprint(1, "Configuration loaded!\n");

        json_object_set(config, "temp", json_object());

        json_t* repositories = json_object_get(config, "repositories");
        for (int i = 0; i < json_array_size(repositories); i++) {
            char* hostname = strdup(json_string_value(json_array_get(repositories, i)));

            FILE* temp = fopen(APPS_DIR "/temp.json", "w+");

            sandia s_info = sandia_create(hostname, 80);
            sandia_add_header(&s_info, "User-Agent", USERAGENT);
            sandia_get_request(&s_info, "/api/v3/information", true, false);

            sandia_ext_write_to_file(&s_info, temp);
            fseek(temp, 0, SEEK_SET);
            sandia_close(&s_info);

            json_error_t error;
            json_t* information = json_loadf(temp, 0, &error);
            if (!information) {
                printf("JSON error on line %d: %s\nWhile syncing repository %s, possibly a network error?\n", error.line, error.text, hostname);
                
                fclose(temp);
                free(hostname);
                home_exit(true);
            }

            int appsamount = json_integer_value(json_object_get(information, "available_apps_count"));

            json_t* name = json_object_get(information, "name");
            printf("\n");
            logprint(0, "Syncing repository ");
            printf("%s\n", json_string_value(name));
            logprint(0, "-------------------");
            for (int i = 0; i < json_string_length(name); i++) {
                printf("-");
            }
            printf("\n");

            json_t* description = json_object_get(information, "description");
            logprint(0, "Description: ");
            printf("%s\n", json_string_value(description));
            
            json_t* provider = json_object_get(information, "provider");
            logprint(0, "Provided by ");
            printf("%s\n", json_string_value(provider));

            char* directory = malloc(strlen(REPO_DIR) + 1 + strlen(hostname) + 1 + 5 + 2); // plus slash plus nul plus .json
            sprintf(directory, "%s/D_%s", REPO_DIR, hostname);
            if (mkdir(directory, 0660) != 0) {
                printf("Cannot create directory %s.\n", directory);
                free(directory);
                json_decref(information);
                fclose(temp);
                free(hostname);
                home_exit(true);
            }
            
            int directoryLen = strlen(directory);
            directory[directoryLen] = '.';
            directory[directoryLen + 1] = 'j';
            directory[directoryLen + 2] = 's';
            directory[directoryLen + 3] = 'o';
            directory[directoryLen + 4] = 'n';
            directory[directoryLen + 5] = '\0';
            
            int variationIndex = strlen(REPO_DIR) + 1;
            directory[variationIndex] = 'I';

            if (json_dump_file(information, directory, 0) != 0) {
                printf("Cannot write to file %s.\n", directory);
                free(directory);
                json_decref(information);
                fclose(temp);
                free(hostname);
                home_exit(true);
            }

            directory[variationIndex] = 'D';
            directory[directoryLen] = '\0';

            json_t* categories = json_object_get(information, "available_categories");
            int categoryamount = json_array_size(categories);
            for (int j = 0; j < categoryamount; j++) {
                json_t* category = json_array_get(categories, j);
                const char* categoryname = json_string_value(json_object_get(category, "name"));

                char* categorynamepath = malloc(strlen(directory) + 1 + strlen(categoryname) + 1); // plus slash  plus nul
                sprintf(categorynamepath, "%s/%s", directory, categoryname);

                /*
                if (json_dump_file(category, categorynamepath, 0) != 0) {
                    printf("Failed writing %s.\n", categorynamepath);
                    free(categorynamepath);
                    free(directory);
                    free(hostname);
                    json_decref(information);
                    fclose(temp);
                    home_exit(true);
                }
                
                // place nul instead of dot in file extension,
                // turning it into a folder path (with no trailing /)
                categorynamepath[strlen(categorynamepath) - 5] = '\0';
                */

                if (fseek(temp, 1, SEEK_CUR) == 0) {
                    fseek(temp, -1, SEEK_CUR);
                    fprintf(temp, "\n");
                }

                if (mkdir(categorynamepath, 0660) != 0) {
                    printf("Failed creating directory %s.\n", categorynamepath);
                    free(categorynamepath);
                    free(directory);
                    free(hostname);
                    json_decref(information);
                    fclose(temp);
                    home_exit(true);
                }

                free(categorynamepath);
            }
            logprint(0, "Processed ");
            printf("%d categor", categoryamount);
            if (categoryamount == 1) printf("y\n");
            else printf("ies\n");
            
            json_decref(information);

            sandia s_cont = sandia_create(hostname, 80);
            sandia_add_header(&s_cont, "User-Agent", USERAGENT);
            sandia_get_request(&s_cont, "/api/v3/contents", true, false);

            fseek(temp, 0, SEEK_SET);
            sandia_ext_write_to_file(&s_cont, temp);
            fseek(temp, 0, SEEK_SET);
            sandia_close(&s_cont);

            json_t* contents = json_loadf(temp, JSON_DISABLE_EOF_CHECK, &error);
            if (!contents) {
                printf("JSON error on line %d: %s\nWhile obtaining content of %s - possibly a network error?\n", error.line, error.text, hostname);
                fclose(temp);
                free(hostname);
                free(directory);
                home_exit(true);
            }

            for (int j = 0; j < appsamount; j++) {
                json_t* app = json_array_get(contents, j);
                const char* slug = json_string_value(json_object_get(app, "slug"));
                const char* categoryname = json_string_value(json_object_get(app, "category"));
                char* slugloc = malloc(strlen(directory) + 1 + strlen(categoryname) + 1 + strlen(slug) + 6); // plus slash plus slash plus .json plus nul
                sprintf(slugloc, "%s/%s/%s.json", directory, categoryname, slug);

                if (json_dump_file(app, slugloc, 0) != 0) {
                    printf("Failed writing %s.\n", slugloc);
                    json_decref(contents);
                    free(slugloc);
                    free(directory);
                    free(hostname);
                    fclose(temp);
                    home_exit(true);
                }

                free(slugloc);
            }

            logprint(0, "Processed ");
            printf("%d app", appsamount);
            if (appsamount != 1) printf("s");
            printf("\n");

            json_decref(contents);
            free(directory);
            fclose(temp);
            free(hostname);
        }

        printf("\n");
        logprint(1, "Syncing complete!\n");
        #endif

        start_tui(config);
        
        clear_screen();
        printf("Exiting...\n");

        json_decref(config);
        exit(0);
}
