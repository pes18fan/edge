#include "edge.h"

#include <string.h>

#include "argparse.h"
#include "command.h"
#include "debug.h"

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
        .changed = false,
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
    if (sf == NULL)
        die(state, "fopen");

    if (is_tfile_empty(state)) {
        fclose(sf);
        return 0;
    }

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
    state->changed = false;

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

    state->changed = true;
}

bool has_unsaved_changes(state_t *state)
{ return state->changed && !is_tfile_empty(state); }

bool is_whitespace(char c)
{ return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\r'); }

void run_edge(state_t *state)
{
    char buf[LINE_MAX];
    bool got_quit_request;

retry:
    got_quit_request = false;
    while (fgets(buf, sizeof buf, stdin) != NULL) {
        // trim all trailing whitespace
        size_t start = strcspn(buf, "\n");
        for (size_t i = start; i > 0 && is_whitespace(buf[i]); i--)
            buf[i] = '\0';

        if (do_command(state, buf) == 1) {
            got_quit_request = true;
            break;
        }
    }

    if (feof(stdin) || got_quit_request) {
        // the if branch handles the 'are you sure you want to quit' logic
        if (has_unsaved_changes(state)) {
            state->changed = false;
            error(state);
            goto retry;
        } else {
            return;
        }
    }

    die(state, "fgets");
}

int main(int argc, const char *argv[])
{
    struct Argparser *p =
        ap_make_parser(NULL, argv[0], "line editor", 'n', NULL);

    bool silent;
    ap_add_flag(p, "--silent", "-s", "Suppress diagnostics", 'b', &silent);

    if (ap_parse(p, argc, argv) == -1) {
        ap_destroy_parser(p);
        return 1;
    }

    // done with parsing args
    ap_destroy_parser(p);

    state_t state = make_state(NULL, !silent);
    run_edge(&state);
    destroy_state(&state);

    return 0;
}
