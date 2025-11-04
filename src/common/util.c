#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "platform.h"

char** split(const char* s, char delim, int* count) {
    if (!s || !count) return NULL;
    
    /* Count delimiters to estimate array size */
    *count = 1;
    for (const char* p = s; *p; p++) {
        if (*p == delim) (*count)++;
    }
    
    /* Allocate array of pointers */
    char** result = malloc(*count * sizeof(char*));
    if (!result) return NULL;
    
    /* Split the string */
    int idx = 0;
    const char* start = s;
    const char* end = s;
    
    while (*end) {
        if (*end == delim) {
            int len = end - start;
            result[idx] = malloc(len + 1);
            if (!result[idx]) {
                /* Cleanup on failure */
                for (int i = 0; i < idx; i++) free(result[i]);
                free(result);
                return NULL;
            }
            strncpy(result[idx], start, len);
            result[idx][len] = '\0';
            idx++;
            start = end + 1;
        }
        end++;
    }
    
    /* Handle the last part */
    int len = end - start;
    result[idx] = malloc(len + 1);
    if (!result[idx]) {
        /* Cleanup on failure */
        for (int i = 0; i < idx; i++) free(result[i]);
        free(result);
        return NULL;
    }
    strncpy(result[idx], start, len);
    result[idx][len] = '\0';
    
    return result;
}

void free_split_result(char** result, int count) {
    if (!result) return;
    for (int i = 0; i < count; i++) {
        if (result[i]) free(result[i]);
    }
    free(result);
}

char* now_time(void) {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    
    char* time_str = malloc(20); /* "YYYY-MM-DD HH:MM:SS" + null terminator */
    if (!time_str) return NULL;
    
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return time_str;
}

uint64_t ms_since(uint64_t t0) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER ui;
    GetSystemTimeAsFileTime(&ft);
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    uint64_t now = (ui.QuadPart - 116444736000000000ULL) / 10000; // Convert to milliseconds
    return now - t0;
#else
    TIME_TYPE tb;
    GET_TIME(tb);
    uint64_t now = MILLISECONDS(tb);
    return now - t0;
#endif
}
