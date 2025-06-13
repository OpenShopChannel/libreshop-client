// debug features for libreshop_client
#include <stdio.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

void debug_pause() {
	while(1) {
		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0)) break;
	}
}

void debug_npause() {
	printf("Press any button to continue...\n");
	VIDEO_WaitVSync();
	debug_pause();
}
