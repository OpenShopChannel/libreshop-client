#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>

#include <jansson.h>

#include <winyl/winyl.h>
#include <winyl/request.h>
#include <winyl/version.h>
#include <winyl/header.h>

#include "debug.h"
#include "config.h"
#include "tui.h"

#include "default_config_json.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void printheader() {
    printf("\x1b[44m");
    printf("                                                                           \n");
    printf("     LibreShop v%s%.*s\n", VERSION, 59 - strlen(VERSION), "                                                           ");
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
#ifdef DEBUG
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

        DIR* library_check = opendir(APPS_DIR);
        if (!library_check) {
            logprint(0, "Creating " APPS_DIR ".\n");

            char dirs[2][strlen(APPS_DIR) + 1];
            strcpy(dirs[0], "/apps");
            strcpy(dirs[1], APPS_DIR);

            for (int i = 0; i < 2; i++) {
                DIR* check = opendir(dirs[i]);
                if (!check) {
                    if (mkdir(dirs[i], 0600) != 0) {
                        logprint(0, "Creating directory ");
                        printf("%s failed :(\n", dirs[i]);
                        home_exit(true);
                    }
                }
                else closedir(check);
            }
        }

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

        json_object_set(config, "repos", json_object());
        json_t* repos = json_object_get(config, "repos");

        printf("\n");
        logprint(0, "Loading repositories.\n");

        char* winagent = malloc(10 + winyl_version_len() + 1 + 10 + strlen(VERSION));
        char* winagentver = malloc(winyl_version_len() + 1);
        winyl_version(winagentver);
        sprintf(winagent, "libwinyl/%s LibreShop/" VERSION, winagentver);
        free(winagentver);

        json_t* repositories = json_object_get(config, "repositories");
        for (int i = 0; i < json_array_size(repositories); i++) {
            char* hostname = strdup(json_string_value(json_array_get(repositories, i)));
            winyl host = winyl_open(hostname, 80);
            winyl_add_header(&host, "User-Agent", winagent);
            
            winyl_response w_info = winyl_request(&host, "/api/v3/information", 0);

            json_error_t error;
            json_t* information = json_loads(w_info.body, 0, &error);
            winyl_response_close(&w_info);

            if (!information) {
                logprint(0, "JSON ");
                printf("error on line %d: %s\nWhile syncing repository %s, possibly a network error?\n", error.line, error.text, hostname);
                home_exit(true);
            }

            winyl_response w_cont = winyl_request(&host, "/api/v3/contents", 0);

            json_t* contents = json_loads(w_cont.body, 0, &error);
            winyl_response_close(&w_cont);
            winyl_close(&host);

            if (!contents) {
                logprint(0, "JSON ");
                printf("error on line %d: %s\nWhile obtaining content of %s - possibly a network error?\n", error.line, error.text, hostname);
                home_exit(true);
            }

            json_object_set(repos, hostname, json_object());
            json_t* repo = json_object_get(repos, hostname);
            json_object_set(repo, "information", information);
            
            json_object_set(repo, "contents", json_object());
            json_t* cont = json_object_get(repo, "contents");
            
            json_t* categories = json_object_get(information, "available_categories");
            for (int j = 0; j < json_array_size(categories); j++) {
                json_t* category = json_array_get(categories, j);
                json_object_set(cont, json_string_value(json_object_get(category, "name")), json_array());
            }

            for (int j = 0; j < json_array_size(contents); j++) {
                json_t* app = json_array_get(contents, j);
                json_array_append(json_object_get(cont, json_string_value(json_object_get(app, "category"))), app);
            }

            logprint(1, "Loaded repository ");
            printf("%s\n", json_string_value(json_object_get(information, "name")));
            logprint(0, "provided by ");
            printf("%s\n", json_string_value(json_object_get(information, "provider")));
            
            json_decref(information);
            json_decref(contents);
            free(hostname);
        }

        start_tui(config, winagent);
        
        free(winagent);
        clear_screen();
        printf("Exiting...\n");

        json_decref(config);
        exit(0);
}
