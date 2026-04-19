/*
 * utils.h
 * General-purpose utilities: timing, error handling, memory helpers.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Timing ---------- */

/* Start a high-resolution timer; returns an opaque start timestamp (seconds). */
double timerStart(void);

/* Return elapsed seconds since the value returned by timerStart(). */
double timerElapsed(double start);

/* Return current wall-clock time in milliseconds (monotonic). */
double getTimeMs(void);

/*
 * Print "<label>: <elapsed> ms" to stdout, where elapsed is computed as
 * getTimeMs() - startMs.
 * Example: printTimeCost("Seed generation", t0);
 */
void printTimeCost(const char *label, double startMs);

/* ---------- Error / validation helpers ---------- */

/*
 * Print a formatted error message to stderr and exit with code 1.
 * Usage: fatalError("could not open file: %s", path);
 */
void fatalError(const char *fmt, ...);

/*
 * Print a warning to stderr (does not exit).
 */
void warnMsg(const char *fmt, ...);

/* ---------- Memory helpers ---------- */

/* malloc that calls fatalError on allocation failure. */
void *safeMalloc(size_t size);

/* calloc that calls fatalError on allocation failure. */
void *safeCalloc(size_t count, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
