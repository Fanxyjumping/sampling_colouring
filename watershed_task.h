/*
 * watershed_task.h
 * Task 1 – OpenCV watershed wrapper.
 *
 * Responsible only for running the segmentation algorithm and producing
 * the markers matrix.  Visualisation functions live in ui.h / ui.cpp.
 */

#ifndef WATERSHED_TASK_H
#define WATERSHED_TASK_H

#include "seed.h"
#include <opencv2/core.hpp>

/*
 * runWatershed – build the markers matrix and invoke cv::watershed.
 *
 * Parameters:
 *   src         – original BGR image (CV_8UC3).  Must be non-empty.
 *                 cv::watershed requires a non-const reference internally.
 *   seeds       – array of seed points, length = seedCount.
 *   seedCount   – number of seeds; must be > 0.
 *   markers_out – output CV_32SC1 matrix (same size as src).
 *                 Each pixel holds its region ID (1-based) after the call,
 *                 or -1 on watershed boundary pixels.
 */
void runWatershed(cv::Mat &src,
                  SeedPoint *seeds, int seedCount,
                  cv::Mat &markers_out);

#endif /* WATERSHED_TASK_H */
