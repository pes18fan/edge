#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "C11 support required"
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include "argparse.h"
#include "debug.h"

#define SAVED_FILE_LEN_MAX 128
#define LINE_MAX 128
#define ZERO_INITIAL_CAP 16

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

    // In 'insert mode'?
    bool inserting;

    // For counting how much got written
    int count;

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
        .inserting = false,
        .verbose = verbose,
    };
    state.zero[0] = 0;
    state.zero[1] = 0;
    state.dot = &state.zero[1];
    state.dol = state.dot;

    if (savedfile != NULL) {
        for (int i = 0; i < SAVED_FILE_LEN_MAX; i++) {
            if (!(*savedfile))
                break;
            state.savedfile[i] = savedfile[i];
        }
    } else {
        memset(state.savedfile, 0, sizeof state.savedfile);
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
    puts("fatal error occured, dying");
    destroy_state(state);
    exit(1);
}

// Assumes state->savedfile is a valid string filename
// Returns number of bytes written
int write_to_sfile(state_t *state)
{
    int *end = state->dol;
    int *curr = state->zero + 1;  // first of zero is reserved
    if (curr == end)
        return 0;

    FILE *sf = fopen(state->savedfile, "w");
    if (sf == NULL)
        die(state, "fopen");

    // Send tfile back to start
    rewind(state->tfile);

    int acc = 0;
    int next_line_pos = 0;
    do {
        // if not the next line we gotta jump around
        if (*curr != next_line_pos) {
            fseek(state->tfile, *curr, SEEK_SET);
            next_line_pos = *curr;
        }

        char buf[LINE_MAX];
        if (fgets(buf, sizeof buf, state->tfile) == NULL) {
            if (!feof(state->tfile)) {
                // ferror always true here
                fclose(sf);
                die(state, "fgets");
            }
        }

        int wrote = fprintf(sf, "%s", buf);
        if (acc < 0) {
            fclose(sf);
            die(state, "fprintf");
        }

        acc += wrote;
        next_line_pos += acc + 1;

        curr++;
    } while (curr != end);

    fclose(sf);

    // Send tfile back to original location
    fseek(state->tfile, *state->dot, SEEK_SET);

    return acc;
}

void write_to_tfile(state_t *state, const char *s)
{
    int w = fprintf(state->tfile, "%s\n", s);
    if (w < 0)
        die(state, "fprintf");

    state->tfline++;
    state->dot++;
    *state->dot = w;
    state->dol++;

    // TODO: make sure zero grows properly when it runs out of capacity
}

void respond(state_t *state, const char *response)
{
    if (state->verbose)
        puts(response);
}

void respondf(state_t *state, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (state->verbose)
        vprintf(fmt, args);

    va_end(args);
}

#define wut() respond(state, "?");

void do_command(state_t *state, const char *input)
{
    if (input == NULL || !(*input)) {
        wut();
        return;
    }

    char c = input[0];

    // when inserting, `input` is text to be written
    // otherwise it is a command
    if (state->inserting) {
        // exit insert mode if got '.'
        if (c == '.') {
            state->inserting = false;
        } else {
            write_to_tfile(state, input);
        }
    } else {

        switch (c) {
        case 'i':
            state->inserting = true;
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
                    state->savedfile[i - 2] = input[i];
                }
            }

            int cnt = write_to_sfile(state);
            respondf(state, "%d\n", cnt);
            state->count = cnt;
            break;
        default:
            wut();
        }
    }
}

void run_ed(state_t *state)
{
    char buf[LINE_MAX];
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
