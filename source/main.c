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

#include "libs/fshelp.h"
#include "libs/nethelp.h"

#include "debug.h"
#include "config.h"

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

        DIR* library_check = opendir(LIBRARY_DIR);
        if (!library_check) {
            closedir(library_check);
            logprint(0, "Creating library directory.\n");
        
            char dirs[3][strlen(LIBRARY_DIR) + 1];
            strcpy(dirs[0], "/apps");
            strcpy(dirs[1], APPS_DIR);
            strcpy(dirs[2], LIBRARY_DIR);

            for (int i = 0; i < 3; i++) {
                DIR* check = opendir(dirs[i]);
                if (!check) {
                    closedir(check);

                    if (mkdir(dirs[i], 0600) != 0) {
                        char* dirmsg = malloc(strlen(dirs[i]) + 30);
                        sprintf(dirmsg, "Creating directory %s failed :(\n", dirs[i]);
                        logprint(2, dirmsg);
                        home_exit(true);
                    }
                }
                else closedir(check);
            }
        }
        else {
            closedir(library_check);
            logprint(0, "Cleaning library directory.\n");
        
            if (delete_directory(LIBRARY_DIR) != 0) {
                logprint(2, "Removing library directory failed :(\n");
                home_exit(true);
            }

            if (mkdir(LIBRARY_DIR, 0600) != 0) {
                logprint(2, "Creating library directory failed :(\n");
                home_exit(true);
            }
        }

        logprint(0, "Downloading library.\n");
        sandia s = sandia_create("libreshop.donut.eu.org", 80);
        sandia_get_request(&s, "/library.zip", true, false);

        FILE* library = fopen(LIBRARY_FILE, "w");
        sandia_ext_write_to_file(&s, library);
        fclose(library);

        sandia_close(&s);

        logprint(1, "Library downloaded!\n");

        debug_npause();
	
        return 0;
}
