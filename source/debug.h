#ifndef DEBUG

// Uncomment if this is a development build
#define DEBUG

#endif

#if ! defined(DEBUG_H) && defined(DEBUG)
#define DEBUG_H

void debug_pause();
void debug_npause();

#endif
