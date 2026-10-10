#include "command.h"

#include <stdarg.h>
#include <string.h>

#include "edge.h"

void respond(state_t *state, const char *response)
{
    if (state->verbose)
        puts(response);
}

void respondf(state_t *state, const char *fmt, ...)
{
    if (state->verbose) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

#define error(state) respond(state, "?");

// Returns 0 in all cases except when quitting via the 'q' or 'Q' commands, in
// which case it returns 1
int do_command(state_t *state, const char *input)
{
#define error_and_return(state) \
    do {                        \
        error(state);           \
        return 0;               \
    } while (0)

    if (input == NULL || !(*input)) {
        error_and_return(state);
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
                error_and_return(state);
            }

            state->inserting = true;
            break;
        case 'w':
            if (!(*state->savedfile)) {
                if (arg == NULL) {
                    error_and_return(state);
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
        case 'q':
            if (arg != NULL) {
                error_and_return(state);
            }

            return 1;
        case 'Q':
            if (arg != NULL) {
                error_and_return(state);
            }

            // clear changed flag to force quit
            state->changed = false;
            return 1;
        case '.': {
            if (arg != NULL) {
                error_and_return(state);
            }

            if (is_tfile_empty(state)) {
                error_and_return(state);
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
                error_and_return(state);
            }

            if (is_tfile_empty(state)) {
                error_and_return(state);
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
            error(state);
        }
    }

    return 0;
#undef error_and_return
}
