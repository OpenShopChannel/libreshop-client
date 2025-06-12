#ifndef TUI_H
#define TUI_H

void logprint(int type, char *message);
void start_tui(json_t* config, char* user_agent);
void clear_screen();

#endif
