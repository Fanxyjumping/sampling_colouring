/*
 * ui.cpp
 * Visualisation UI helpers implementation.
 */

#include "ui.h"
#include "utils.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>

/* ── visualizeWatershed ───────────────────────────────────────────── */

cv::Mat visualizeWatershed(cv::Mat &src, cv::Mat &markers, int regionCount) {

    /* ── Validity checks ─────────────────────────────────────────── */
    if (src.empty())
        fatalError("visualizeWatershed: src image is empty.");
    if (regionCount <= 0)
        fatalError("visualizeWatershed: regionCount must be > 0 (got %d).",
                   regionCount);

    /* ── Step 1: Assign one random colour per region ─────────────────
     *  srand(42) ensures the same colour palette is produced every run,
     *  making visualisation results reproducible.
     *  Index 0 is unused (background / boundary placeholder).         */
    srand(42);
    std::vector<cv::Vec3b> regionColor((size_t)(regionCount + 1));
    for (int i = 1; i <= regionCount; ++i) {
        regionColor[i] = cv::Vec3b(
            (uchar)(rand() % 256),   /* B */
            (uchar)(rand() % 256),   /* G */
            (uchar)(rand() % 256));  /* R */
    }

    /* ── Step 2: Build a full-colour mask from the markers matrix ────
     *  Interior pixels (label >= 1) get their region colour.
     *  Boundary pixels (label == -1) stay black (default zero).       */
    cv::Mat mask(src.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    for (int r = 0; r < markers.rows; ++r) {
        const int  *mrow = markers.ptr<int>(r);
        cv::Vec3b  *drow = mask.ptr<cv::Vec3b>(r);
        for (int c = 0; c < markers.cols; ++c) {
            int label = mrow[c];
            if (label > 0 && label <= regionCount)
                drow[c] = regionColor[label];
            /* label == -1 → boundary: pixel stays (0,0,0) */
        }
    }

    /* ── Step 3: Blend original and mask at 50% / 50% ───────────────
     *  result = 0.5 * src + 0.5 * mask                               */
    cv::Mat result;
    cv::addWeighted(src, 0.5, mask, 0.5, 0.0, result);

    return result;
}

/* ── drawSeeds ────────────────────────────────────────────────────── */

void drawSeeds(cv::Mat &img, SeedPoint *seeds, int seedCount) {

    /* ── Validity checks ─────────────────────────────────────────── */
    if (img.empty())
        fatalError("drawSeeds: image is empty.");
    if (!seeds || seedCount <= 0)
        fatalError("drawSeeds: invalid seeds array or seedCount.");

    for (int i = 0; i < seedCount; ++i) {
        cv::Point pt(seeds[i].x, seeds[i].y);

        /* Filled red circle (radius 3) marks the seed location */
        cv::circle(img, pt, 3, cv::Scalar(0, 0, 255), cv::FILLED);

        /* ID label: only when K is small enough to remain readable */
        if (seedCount <= 200) {
            /* Offset text slightly to avoid overlapping the circle */
            cv::Point textPt(pt.x + 4, pt.y - 4);
            cv::putText(img,
                        std::to_string(seeds[i].id),
                        textPt,
                        cv::FONT_HERSHEY_PLAIN,
                        0.7,                       /* font scale  */
                        cv::Scalar(255, 255, 0),   /* yellow text */
                        1,
                        cv::LINE_AA);
        }
    }
}

/* ── renderFourColor ──────────────────────────────────────────────── */

void renderFourColor(const cv::Mat &markers,
                     int region_count,
                     const int *color_out,
                     cv::Mat &vis_out) {
    /* Four visually distinct BGR colours mapped to colour IDs 0-3 */
    /* OpenCV stores pixels as BGR, so each entry is {B, G, R} */
    static const cv::Vec3b palette[4] = {
        { 60,  20, 220},  /* crimson red  – BGR(60,20,220)   */
        {255, 144,  30},  /* dodger blue  – BGR(255,144,30)  */
        { 50, 205,  50},  /* lime green   – BGR(50,205,50)   */
        {  0, 215, 255},  /* gold         – BGR(0,215,255)   */
    };

    vis_out = cv::Mat::zeros(markers.size(), CV_8UC3);

    for (int r = 0; r < markers.rows; ++r) {
        const int  *mrow = markers.ptr<int>(r);
        cv::Vec3b  *drow = vis_out.ptr<cv::Vec3b>(r);
        for (int c = 0; c < markers.cols; ++c) {
            int label = mrow[c];
            if (label > 0 && label <= region_count) {
                int col = color_out[label];
                if (col >= 0 && col < 4)
                    drow[c] = palette[col];
            }
            /* label == -1 (boundary) → stays black */
        }
    }
}

/* ── showImage ────────────────────────────────────────────────────── */

void showImage(const char *window_title, const cv::Mat &img) {
    cv::namedWindow(window_title, cv::WINDOW_NORMAL);
    cv::imshow(window_title, img);
    cv::waitKey(0);
    cv::destroyWindow(window_title);
}

/* ── printStatus ──────────────────────────────────────────────────── */

void printStatus(int K, int actual_seeds,
                 double seed_time, double ws_time,
                 double color_time, double failure_rate) {
    printf("------------------------------------------------------------\n");
    printf("  Requested K          : %d\n",     K);
    printf("  Seeds generated      : %d\n",     actual_seeds);
    printf("  Seed generation time : %.4f s\n", seed_time);
    printf("  Watershed time       : %.4f s\n", ws_time);
    if (color_time >= 0.0)
        printf("  Four-colour time     : %.4f s\n", color_time);
    if (failure_rate >= 0.0)
        printf("  Backtrack fail rate  : %.2f%%\n", failure_rate * 100.0);
    printf("------------------------------------------------------------\n");
}
