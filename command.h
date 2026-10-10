#ifndef edge_command_h
#define edge_command_h

#include "edge.h"

void respond(state_t *state, const char *response);
void respondf(state_t *state, const char *fmt, ...);

#define error(state) respond(state, "?");

// Returns 0 in all cases except when quitting via the 'q' or 'Q' commands, in
// which case it returns 1
int do_command(state_t *state, const char *input);

#endif
