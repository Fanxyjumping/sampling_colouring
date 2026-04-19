/*
 * ui.h
 * Visualisation UI helpers: watershed overlay, seed annotation,
 * four-colour rendering, image display, and console status output.
 */

#ifndef UI_H
#define UI_H

#include "seed.h"
#include "adjacency.h"
#include <opencv2/core.hpp>

/* ── Watershed visualisation ──────────────────────────────────────── */

/*
 * visualizeWatershed – create a 50/50 blended visualisation of the
 * watershed result.
 *
 * Each region (ID 1..regionCount) is assigned a random BGR colour using
 * a fixed seed (srand(42)) so results are reproducible across runs.
 * Boundary pixels (markers == -1) are rendered black.
 *
 * Parameters:
 *   src         – original BGR image (CV_8UC3). Must be non-empty.
 *   markers     – CV_32SC1 markers matrix from runWatershed().
 *   regionCount – total number of watershed regions.
 *
 * Returns the blended image (same size and type as src).
 */
cv::Mat visualizeWatershed(cv::Mat &src, cv::Mat &markers, int regionCount);

/*
 * drawSeeds – annotate an image with seed point markers in-place.
 *
 * Each seed is drawn as a filled red circle (radius 3).
 * When seedCount <= 200, the region ID is printed next to the circle;
 * at higher densities the text is omitted to avoid visual clutter.
 *
 * Parameters:
 *   img       – BGR image to annotate (modified in-place).
 *   seeds     – seed point array.
 *   seedCount – length of the seeds array.
 */
void drawSeeds(cv::Mat &img, SeedPoint *seeds, int seedCount);

/* ── Four-colour visualisation ────────────────────────────────────── */

/*
 * renderFourColor – render the four-colour result as a solid-colour image.
 *
 * Maps colour IDs 0-3 to four visually distinct BGR values and fills each
 * pixel with its region's assigned colour.
 * Watershed boundaries (markers == -1) are rendered as black.
 *
 * Parameters:
 *   markers     – CV_32SC1 watershed markers.
 *   region_count – number of regions.
 *   color_out   – colour assignment array (1-indexed, values 0-3).
 *   vis_out     – output BGR image (same size as markers).
 */
void renderFourColor(const cv::Mat &markers,
                     int region_count,
                     const int *color_out,
                     cv::Mat &vis_out);

/* ── General display / status ─────────────────────────────────────── */

/*
 * showImage – display an image in a named OpenCV window and wait for a
 * key press.  The window is destroyed afterwards.
 */
void showImage(const char *window_title, const cv::Mat &img);

/*
 * printStatus – print a formatted status summary to stdout.
 * Pass -1 for color_time / failure_rate if Task 2 has not been run yet.
 */
void printStatus(int K, int actual_seeds,
                 double seed_time, double ws_time,
                 double color_time, double failure_rate);

#endif /* UI_H */
