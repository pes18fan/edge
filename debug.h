#ifndef edge_debug_h
#define edge_debug_h

#define todo()                                                     \
    do {                                                           \
        fprintf(stderr, "crashing, this code is not yet done!\n"); \
        fprintf(stderr, "at line: %d\n", __LINE__);                \
        exit(1);                                                   \
    } while (0);

#define __debugf(fmt, ...)            \
    do {                              \
        printf("DEBUG: ");            \
        printf((fmt), ##__VA_ARGS__); \
    } while (0)

#define __debugfln(fmt, ...)            \
    do {                                \
        __debugf((fmt), ##__VA_ARGS__); \
        printf("\n");                   \
    } while (0)

#endif
