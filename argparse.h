/* argparse: Argument parsing library inspired by the Python library of the same
 * name */

#ifndef argparse_h
#define argparse_h

#define MAX_FLAGS 16
#define MAX_SUBPARSERS 16

struct Flag {
    // Name of the flag.
    const char *name;

    // Short form of the flag. Can be NULL, if the flag does not have one.
    const char *short_name;

    // Description. Can be left NULL to not provide any of it.
    const char *description;

    // Argument taken by the flag, it points to a specified storage location by
    // the user.
    // Avoid accessing this data directly.
    void *_arg;

    // A character representing the type of _arg. It can be one of:
    //     - 'i': int
    //     - 's': string (const char *)
    //     - 'b': boolean (bool or any integer type)
    //     - 'n': none (no argument at all)
    // A point about 'b': it is similar to 'n' in how it is parsed in the
    // sense that it doesn't actually read any command line arg for its value.
    // The presence of the argument is enough for the value to be true.
    char arg_kind;
};

// Parser for a command. Has a set of its own subparsers.
// and flags.
struct Argparser {
    // Name of the command.
    const char *name;

    // Description. Can be left NULL to not provide any of it.
    const char *description;

    // Subparsers to parse any subcommands.
    struct Argparser *subparsers[MAX_SUBPARSERS];
    int subparser_count;

    // Flags.
    struct Flag *flags[MAX_FLAGS];
    int flag_count;

    // Argument taken by the flag, it points to a specified storage location by
    // the user.
    // Avoid accessing this data directly.
    void *_arg;

    // A character representing the type of _arg. It can be one of:
    //     - 'i': int
    //     - 's': string (const char *)
    //     - 'b': boolean (bool or any integer type)
    //     - 'n': none (no argument at all)
    // A point about 'b': it is similar to 'n' in how it is parsed in the
    // sense that it doesn't actually read any command line arg for its value.
    // The presence of the argument is enough for the value to be true.
    char arg_kind;
};

// Create an argument parser as the subparser of `parent`, with a subcommand
// name `name`, description `description`, and argument kind `kind`.
// If the parser is the root parser (has no parents), `parent` must be set
// to `NULL`.
// The argument the command takes is provided via `kind` (described in the
// definition of `struct Argparser`), alongside the storage region for that
// value provided in `arg_ptr`. If the command takes no argument (`kind` ==
// 'n'), `arg_ptr` can be set to `NULL`. If the provided parser spec has any
// issues (e.g. attempt to add a duplicate subcommand, attempt to add a
// subparser for a parser that takes an argument, addition of too many
// subparsers), the function prints the error to stderr and exits the program
// with status 1.
// `arg_ptr` will be one of these respective values in the case the subcommand
// is never provided (and thus no argument ever parsed):
//      - 'i': -0xdeadbeef
//      - 's': "\0"
//      - 'b': false
struct Argparser *ap_make_parser(struct Argparser *parent, const char *name,
                                 const char *description, char kind,
                                 void *arg_ptr);

// Deallocate an Argparser.
// Also deallocates all of its subparsers and flags; hence you should avoid
// running it for every single subparser; just destroy the root.
void ap_destroy_parser(struct Argparser *parent);

// Add a flag to `parser`, with a `name` and optional `short_name` and
// `description`. The latter two can be left NULL if undesired. The type of the
// argument the flag takes is provided via `kind` (described in the definition
// of `struct Argparser`), alongside the storage region for that value provided
// in `arg_ptr`. If the flag takes no argument (`kind` == 'n'), `arg_ptr` can be
// set to `NULL`.
// If the provided flag spec has any issues (e.g. invalid flag name, attempt to
// add a duplicate flag, addition of too many flags, invalid argument kind), the
// function prints the error to stderr and exits the program with status 1.
// `arg_ptr` will be one of these respective values in the case the flag is
// never provided (and thus no argument ever parsed):
//      - 'i': -0xdeadbeef
//      - 's': "\0"
//      - 'b': false
void ap_add_flag(struct Argparser *parser, const char *name,
                 const char *short_name, const char *description, char kind,
                 void *arg_ptr);

// Parse arguments based on the spec in `parser`.
// Returns the number of arguments parsed on success (a non-negative integer),
// otherwise returns -1.
// If it encounters a problem with the setup of the arguments (for example an
// invalid argument kind, or invalid `argv`), it prints the issue to stderr and
// exits the program with status 1.
int ap_parse(struct Argparser *parser, int argc, const char *argv[]);

#endif
