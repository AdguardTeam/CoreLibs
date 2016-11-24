#ifndef TEST_H
#define TEST_H

#define assert(X) do {\
    if (!(X)) {\
        fprintf(stderr, "assertion failed: " ##X);\
        exit(1);\
    }\
} while (0)

#endif /* TEST_H */