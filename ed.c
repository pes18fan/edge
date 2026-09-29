#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include "argparse.h"

#define SAVED_FILE_LEN_MAX 128
#define ZERO_INITIAL_CAP 16

#define todo()                                                     \
    do {                                                           \
        fprintf(stderr, "crashing, this code is not yet done!\n"); \
        fprintf(stderr, "at line: %d\n", __LINE__);                \
        exit(1);                                                   \
    } while (0);

typedef struct {
    // Temporary file where the data is stored while editing
    FILE *tfile;
    int tfline;

    // File to actually commit data to
    char savedfile[SAVED_FILE_LEN_MAX];

    // Dynamic array storing byte offsets of each line
    int *zero;
    int zero_cap;

    int *dot;  // Pointer to current line (in zero)
    int *dol;  // Pointer to last line (in zero)

    // Should ed be verbose?
    bool verbose;
} state_t;

state_t make_state(const char *savedfile, bool verbose)
{
    state_t state = {
        .tfile = tmpfile(),
        .tfline = 1,
        .savedfile = "",
        .zero = malloc(ZERO_INITIAL_CAP),
        .zero_cap = ZERO_INITIAL_CAP,
        .dot = NULL,
        .dol = NULL,
        .verbose = verbose,
    };
    state.zero[0] = 0;
    state.zero[1] = 0;
    state.dot = state.zero + 1;
    state.dol = state.dot;

    if (savedfile != NULL)
        for (int i = 0; i < SAVED_FILE_LEN_MAX; i++) {
            if (!(*savedfile))
                break;
            state.savedfile[i] = savedfile[i];
        }

    return state;
}

void destroy_state(state_t *state)
{
    fclose(state->tfile);
    free(state->zero);
}

bool is_whitespace(char c)
{ return c == ' ' || c == '\n' || c == '\t' || c == '\v'; }

// Gotta close the temp file!
noreturn void die(state_t *state, const char *perror_msg)
{
    if (perror_msg != NULL)
        perror(perror_msg);
    destroy_state(state);
    exit(1);
}

void write_to_sfile(state_t *state)
{
    (void) state;
    todo();
}

void write_to_tfile(state_t *state, char *s)
{
    int w = fprintf(state->tfile, "%s", s);

    state->tfline++;
    *state->dot = w;
    state->dot++;
    state->dol++;

    // TODO: make sure zero grows properly when it runs out of capacity
}

void start_insert(state_t *state)
{
    char buf[128];

    while (fgets(buf, sizeof buf, stdin) != NULL) {
        if (*buf && buf[0] == '.')
            return;
        write_to_tfile(state, buf);
    }

    if (feof(stdin)) {
        return;
    }

    die(state, "fgets");
}

void respond(state_t *state, const char *response)
{
    if (state->verbose)
        puts(response);
}

#define wut() respond(state, "?");

void do_command(state_t *state, const char *input)
{
    if (input == NULL || !(*input)) {
        wut();
        return;
    }

    char c = input[0];
    switch (c) {
    case 'i':
        start_insert(state);
        break;
    case 'w':
        if (!(*state->savedfile)) {
            size_t len = strlen(input);
            if (len < 2 || !is_whitespace(input[1])) {
                wut();
                return;
            }

            for (size_t i = 2; i < len; i++) {
                if (i + 2 >= SAVED_FILE_LEN_MAX)
                    break;
                state->savedfile[i] = input[i];
            }
        }

        // TODO: write to the savedfile
        write_to_sfile(state);
        break;
    default:
        wut();
    }
}

void run_ed(state_t *state)
{
    char buf[128];
    while (fgets(buf, sizeof buf, stdin) != NULL) {
        // trim all trailing whitespace
        size_t start = strcspn(buf, "\n");
        for (size_t i = start; is_whitespace(buf[i]); i--)
            buf[i] = '\0';

        do_command(state, buf);
    }

    if (feof(stdin)) {
        return;
    }

    die(state, "fgets");
}

int main(int argc, const char *argv[])
{
    struct Argparser *p =
        ap_make_parser(NULL, argv[0], "line editor", 'n', NULL);

    bool silent;
    ap_add_flag(p, "--silent", "-s", "Suppress diagnostics", 'b', &silent);

    if (ap_parse(p, argc, argv) == -1)
        return 1;

    // done with parsing args
    ap_destroy_parser(p);

    state_t state = make_state(NULL, !silent);
    run_ed(&state);
    destroy_state(&state);

    return 0;
}
