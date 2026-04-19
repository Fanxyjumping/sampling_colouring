/*
 * utils.cpp
 * Implementation of general-purpose utilities: timing, error handling,
 * memory wrappers.
 */

#include "utils.h"
#include <stdarg.h>
#include <time.h>

#ifdef __APPLE__
#  include <mach/mach_time.h>
#endif

/* ---------- Timing ---------- */

double timerStart(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

double timerElapsed(double start) {
    return timerStart() - start;
}

/* getTimeMs – wall-clock time in milliseconds (same clock as timerStart) */
double getTimeMs(void) {
    return timerStart() * 1000.0;
}

/* printTimeCost – print "<label>: <elapsed> ms" to stdout */
void printTimeCost(const char *label, double startMs) {
    double elapsed = getTimeMs() - startMs;
    printf("  %-28s %.2f ms\n", label, elapsed);
}

/* ---------- Error / validation helpers ---------- */

void fatalError(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[ERROR] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

void warnMsg(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[WARN] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* ---------- Memory helpers ---------- */

void *safeMalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) fatalError("malloc failed for %zu bytes", size);
    return ptr;
}

void *safeCalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr) fatalError("calloc failed for %zu x %zu bytes", count, size);
    return ptr;
}
