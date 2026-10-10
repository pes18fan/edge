#ifndef edge_edge_h
#define edge_edge_h

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error C11 support required
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#define SAVED_FILE_LEN_MAX 128
#define LINE_MAX 128
#define ZERO_INITIAL_CAP sizeof(int) * 8

typedef struct {
    // Temporary file where the data is stored while editing
    FILE *tfile;
    int tfline;

    // File to actually commit data to
    char savedfile[SAVED_FILE_LEN_MAX];

    // Dynamic array storing byte offsets of each line
    int *zero;
    size_t zero_cap;
    size_t zero_len;

    size_t dot_index;  // Index to current line in zero
    size_t dol_index;  // Index to last line in zero

    // In 'insert mode'?
    bool inserting;

    // Should edge be verbose?
    bool verbose;

    // Did the temp buffer change without those changes being written to
    // savedfile?
    bool changed;
} state_t;

state_t make_state(const char *savedfile, bool verbose);
void destroy_state(state_t *state);

int *dot(state_t *state);
int *dol(state_t *state);

bool is_tfile_empty(state_t *state);

// Gotta close the temp file!
noreturn void die(state_t *state, const char *perror_msg);

// Assumes state->savedfile is a valid string filename
// Returns number of bytes written
int write_to_sfile(state_t *state);

void write_to_tfile(state_t *state, const char *s);

bool has_unsaved_changes(state_t *state);

void run_edge(state_t *state);

bool is_whitespace(char c);

#endif
