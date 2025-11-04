#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

/* Function declarations */
char** split(const char* s, char delim, int* count);
void free_split_result(char** result, int count);
char* now_time(void);
uint64_t ms_since(uint64_t t0);

#endif /* UTIL_H */
