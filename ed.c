#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "argparse.h"

volatile sig_atomic_t quit;

void sigint_handler(int sig)
{
    (void) sig;

    quit = 1;
}

typedef struct {
    FILE *tfile;
    char *savedfile;
    int *zero;
    bool verbose;
} state_t;

state_t make_state(char *savedfile, bool verbosity)
{
    return (state_t){
        .tfile = tmpfile(),
        .savedfile = savedfile,
        .zero = malloc(16),
        .verbose = verbosity,
    };
}

void destroy_state(state_t *state)
{
    fclose(state->tfile);
    free(state->zero);
}

int main(int argc, const char *argv[])
{
    quit = 0;

    struct Argparser *p =
        ap_make_parser(NULL, argv[0], "line editor", 'n', NULL);

    bool not_verbose;
    ap_add_flag(p, "--no-verbose", "-n", "don't be verbose", 'b', &not_verbose);

    if (ap_parse(p, argc, argv) == -1)
        return 1;

    ap_destroy_parser(p);

    struct sigaction sa = {
        .sa_handler = sigint_handler,
        .sa_flags = 0,
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    state_t state = make_state(NULL, !not_verbose);

    while (!quit) {
        puts("welcome to ed...");
        sleep(1);
    }

    printf("bye!");
    destroy_state(&state);
}
