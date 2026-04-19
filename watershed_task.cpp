/*
 * watershed_task.cpp
 * Task 1 – OpenCV watershed wrapper implementation.
 *
 * Steps performed by runWatershed():
 *   1. Validate inputs (non-empty src, seedCount > 0, non-NULL seeds).
 *   2. Create a zero-initialised CV_32SC1 markers matrix (0 = unknown).
 *   3. Stamp each seed's pixel position with its 1-based region ID.
 *   4. Call cv::watershed() – it fills every pixel with a region ID and
 *      marks boundaries as -1, both written back into markers in-place.
 */

#include "watershed_task.h"
#include "utils.h"
#include <opencv2/imgproc.hpp>  /* cv::watershed */

void runWatershed(cv::Mat &src,
                  SeedPoint *seeds, int seedCount,
                  cv::Mat &markers_out) {

    /* ── Step 1: Input validation ──────────────────────────────────── */
    if (src.empty())
        fatalError("runWatershed: src image is empty.");
    if (seedCount <= 0)
        fatalError("runWatershed: seedCount must be > 0 (got %d).", seedCount);
    if (!seeds)
        fatalError("runWatershed: seeds array is NULL.");

    /* ── Step 2: Initialise markers to 0 (unknown / background) ────── */
    markers_out = cv::Mat::zeros(src.size(), CV_32SC1);

    /* ── Step 3: Stamp each seed with its 1-based region ID ─────────
     *  Seeds outside the image bounds are silently skipped.           */
    for (int i = 0; i < seedCount; ++i) {
        int x = seeds[i].x;  /* column */
        int y = seeds[i].y;  /* row    */
        if (x < 0 || x >= src.cols || y < 0 || y >= src.rows) {
            warnMsg("runWatershed: seed id=%d (%d,%d) out of bounds – skipped.",
                    seeds[i].id, x, y);
            continue;
        }
        markers_out.at<int>(y, x) = seeds[i].id;
    }

    /* ── Step 4: Run watershed ──────────────────────────────────────
     *  After this call:
     *    markers_out[r][c] > 0  → region ID of that pixel
     *    markers_out[r][c] == -1 → watershed boundary              */
    cv::watershed(src, markers_out);
}
