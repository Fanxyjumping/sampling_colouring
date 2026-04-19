/*
 * adjacency.h
 * Task 2 – Region adjacency list (RAG) construction from a watershed
 * markers matrix.
 *
 * Strategy:
 *   1. Single O(M×N) scan of markers, checking right/bottom neighbours.
 *   2. A temporary heap-allocated bool matrix deduplicates pairs in O(1).
 *   3. The bool matrix is converted to singly-linked adjacency lists, then freed.
 */

#ifndef ADJACENCY_H
#define ADJACENCY_H

#include <opencv2/core.hpp>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Data structures ────────────────────────────────────────────── */

/* One node in a singly-linked neighbour list. */
typedef struct AdjNode {
    int             neighbor_id; /* 1-based region ID of the neighbour */
    struct AdjNode *next;
} AdjNode;

/*
 * The complete adjacency list for all regions.
 * lists[i] is the head of the neighbour list for region i (i is 1-based).
 * lists[0] is always NULL (unused).
 */
typedef struct {
    int        region_count;
    AdjNode  **lists;
} AdjList;

/* ── API ─────────────────────────────────────────────────────────── */

/*
 * buildAdjList – construct the region adjacency list from a watershed
 * markers matrix.
 *
 * Parameters:
 *   markers     – CV_32SC1 matrix from runWatershed(). Must be non-empty.
 *                 Each pixel holds a region ID (1..regionCount) or -1.
 *   regionCount – total number of watershed regions. Must be > 0.
 *
 * Returns a heap-allocated AdjList; caller must call freeAdjList().
 * Returns NULL on invalid input.
 */
AdjList *buildAdjList(cv::Mat &markers, int regionCount);

/*
 * freeAdjList – release all memory owned by an AdjList (nodes, lists
 * array, and the struct itself).
 */
void freeAdjList(AdjList *adj);

/*
 * getNeighborCount – return the number of neighbours (degree) of a region.
 *
 * Parameters:
 *   adj      – adjacency list (may be NULL → returns 0).
 *   regionId – 1-based region ID.
 *
 * Returns 0 for invalid input.
 */
int getNeighborCount(const AdjList *adj, int regionId);

#ifdef __cplusplus
}
#endif

#endif /* ADJACENCY_H */
