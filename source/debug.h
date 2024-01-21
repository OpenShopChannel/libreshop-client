#ifndef DEBUG

// Uncomment if this is a development build
//#define DEBUG

// Useful for repo stuff
//#define ALWAYS_DEFAULT_CONFIG

#endif

#if ! defined(DEBUG_H) && defined(DEBUG)
#define DEBUG_H

void debug_pause();
void debug_npause();

#endif
