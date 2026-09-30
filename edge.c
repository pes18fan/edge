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

    // For counting how much got written
    int count;

    // Should edge be verbose?
    bool verbose;
} state_t;

int *dot(state_t *state)
{ return &state->zero[state->dot_index]; }

int *dol(state_t *state)
{ return &state->zero[state->dol_index]; }

state_t make_state(const char *savedfile, bool verbose)
{
    FILE *tf = tmpfile();
    if (tf == NULL) {
        perror("tmpfile");
        exit(1);
    }

    int *z = malloc(ZERO_INITIAL_CAP);
    if (z == NULL) {
        perror("malloc");
        fclose(tf);
        exit(1);
    }

    z[0] = 0;
    z[1] = 0;
    int dot_index = 1;
    int dol_index = dot_index;

    state_t state = {
        .tfile = tf,
        .tfline = 1,
        // .savedfile = ...
        .zero = z,
        .zero_cap = ZERO_INITIAL_CAP,
        .zero_len = 1 * sizeof(int),  // as zero[0] is occupied from start
        .dot_index = dot_index,
        .dol_index = dol_index,
        .inserting = false,
        .verbose = verbose,
    };

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
    int *end = dol(state);
    int *curr = state->zero + 1;  // first of zero is reserved
    if (curr == end)
        return 0;

    FILE *sf = fopen(state->savedfile, "w");
    if (sf == NULL)
        die(state, "fopen");

    // Send tfile back to start
    rewind(state->tfile);

    int acc = 0;
    int pos = 0;
    do {
        // if not the next line we gotta jump around
        if (*curr != pos + 1) {
            fseek(state->tfile, *curr, SEEK_SET);
            pos = *curr - 1;
        }

        char buf[LINE_MAX];
        if (fgets(buf, sizeof buf, state->tfile) == NULL) {
            if (!feof(state->tfile)) {
                // ferror always true here
                fclose(sf);
                die(state, "fgets");
            }
        }

        int w = fprintf(sf, "%s", buf);
        if (acc < 0) {
            fclose(sf);
            die(state, "fprintf");
        }

        acc += w;
        pos += w;

        curr++;
    } while (curr != end);

    fclose(sf);

    // Send tfile back to original location
    fseek(state->tfile, *dot(state), SEEK_SET);

    return acc;
}

void write_to_tfile(state_t *state, const char *s)
{
    int w = fprintf(state->tfile, "%s\n", s);
    if (w < 0)
        die(state, "fprintf");

    state->tfline++;
    state->dot_index++;
    state->dol_index++;
    state->zero_len += sizeof(int);

    if (state->zero_len >= state->zero_cap) {
        state->zero_cap *= 2;
        int *res = realloc(state->zero, state->zero_cap);
        if (res == NULL)
            die(state, "realloc");
        state->zero = res;
    }

    *dot(state) = w;
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

// Returns 0 in all cases except when the 'Q' (force quit) command is given,
// in which case it returns 1
int do_command(state_t *state, const char *input)
{
    if (input == NULL || !(*input)) {
        wut();
        return 0;
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
                    return 0;
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
        case 'Q':
            return 1;
        default:
            wut();
        }
    }

    return 0;
}

void run_ed(state_t *state)
{
    char buf[LINE_MAX];
    while (fgets(buf, sizeof buf, stdin) != NULL) {
        // trim all trailing whitespace
        size_t start = strcspn(buf, "\n");
        for (size_t i = start; is_whitespace(buf[i]); i--)
            buf[i] = '\0';

        if (do_command(state, buf) == 1)
            return;
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
