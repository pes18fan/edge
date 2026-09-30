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

    // Should edge be verbose?
    bool verbose;
} state_t;

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
    size_t dot_index = 0;
    size_t dol_index = dot_index;

    state_t state = {
        .tfile = tf,
        .tfline = 0,
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

int *dot(state_t *state)
{ return &state->zero[state->dot_index]; }

int *dol(state_t *state)
{ return &state->zero[state->dol_index]; }

bool is_tfile_empty(state_t *state)
{ return dol(state) == state->zero; }

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
    FILE *sf = fopen(state->savedfile, "w");
    if (sf == NULL) {
        fclose(sf);
        die(state, "fopen");
    }

    if (is_tfile_empty(state))
        return 0;

    int *ptr = state->zero + 1;  // first of zero is reserved
    int *end = dol(state);

    // Send tfile back to start
    rewind(state->tfile);

    int acc = 0;
    while (true) {
        // NOTE: this fseek is a bit redundant in a lot of cases, as often the
        // next offset in zero is right next to the previous; but in the cases
        // where that's not the case this will unambigiously switch
        // still, might hurt perf that way
        fseek(state->tfile, *ptr, SEEK_SET);

        char buf[LINE_MAX];
        if (fgets(buf, sizeof buf, state->tfile) == NULL) {
            if (ferror(state->tfile)) {
                fclose(sf);
                die(state, "fgets");
            }
        }

        int w = fprintf(sf, "%s", buf);
        if (w < 0) {
            fclose(sf);
            die(state, "fprintf");
        }

        acc += w;

        if (ptr == end)
            break;

        ptr++;
    }

    fclose(sf);

    // Send tfile back to original location
    fseek(state->tfile, *dot(state), SEEK_SET);

    return acc;
}

void write_to_tfile(state_t *state, const char *s)
{
    int p = (int) ftell(state->tfile);
    if (fprintf(state->tfile, "%s\n", s) < 0)
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

    *dot(state) = p;
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
#define wut_and_return() \
    do {                 \
        wut();           \
        return 0;        \
    } while (0)

    if (input == NULL || !(*input)) {
        wut_and_return();
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
        // get the argument (if its there)
        const char *arg = NULL;
        size_t arglen = 0;
        size_t len = strlen(input);
        if (len > 2 && is_whitespace(input[1])) {
            for (size_t i = 2; i < len; i++) {
                if (!is_whitespace(input[i])) {
                    arg = input + i;
                    arglen = len - i;
                    break;
                }
            }
        }

        switch (c) {
        case 'i':
            if (arg != NULL) {
                wut_and_return();
            }

            state->inserting = true;
            break;
        case 'w':
            if (!(*state->savedfile)) {
                if (arg == NULL) {
                    wut_and_return();
                }

                for (size_t i = 0; i < arglen; i++) {
                    if (i >= SAVED_FILE_LEN_MAX)
                        break;
                    state->savedfile[i] = arg[i];
                }
            }

            int count = write_to_sfile(state);
            respondf(state, "%d\n", count);
            break;
        case 'Q':
            if (arg != NULL) {
                wut_and_return();
            }

            return 1;
        case '.': {
            if (arg != NULL) {
                wut_and_return();
            }

            if (is_tfile_empty(state)) {
                wut_and_return();
            }

            fseek(state->tfile, *dot(state), SEEK_SET);
            char buf[LINE_MAX];
            if (fgets(buf, sizeof buf, state->tfile) == NULL) {
                if (ferror(state->tfile)) {
                    die(state, "fgets");
                }
            }

            respondf(state, "%s", buf);
            break;
        }
        case '$': {
            if (arg != NULL) {
                wut_and_return();
            }

            if (is_tfile_empty(state)) {
                wut_and_return();
            }

            fseek(state->tfile, *dol(state), SEEK_SET);
            char buf[LINE_MAX];
            if (fgets(buf, sizeof buf, state->tfile) == NULL) {
                if (ferror(state->tfile)) {
                    die(state, "fgets");
                }
            }

            respondf(state, "%s", buf);
            fseek(state->tfile, *dot(state), SEEK_SET);
            break;
        }
        default:
            wut();
        }
    }

    return 0;
#undef wut_and_return
}

void run_edge(state_t *state)
{
    char buf[LINE_MAX];
    while (fgets(buf, sizeof buf, stdin) != NULL) {
        // trim all trailing whitespace
        size_t start = strcspn(buf, "\n");
        for (size_t i = start; i > 0 && is_whitespace(buf[i]); i--)
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
    run_edge(&state);
    destroy_state(&state);

    return 0;
}
