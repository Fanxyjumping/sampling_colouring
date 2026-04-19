/*
 * coloring.h
 * Task 2 – Four-colour graph colouring with BFS ordering and stack-based
 * backtracking.
 *
 * Algorithm overview:
 *   1. Pick the highest-degree region as the BFS start node.
 *   2. BFS from the start → produces a colouring order[] of all regions.
 *   3. Greedy colour assignment in BFS order (colours 0..3).
 *   4. On conflict, backtrack via an explicit stack and try the next
 *      available colour for the previously-coloured region.
 *   5. Fail if total backtrack steps exceed regionCount * 10.
 */

#ifndef COLORING_H
#define COLORING_H

#include "adjacency.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes for fourColorWatershed() — 1 = success, 0 = failure.   */
#define COLOR_SUCCESS  1
#define COLOR_FAILURE  0

/*
 * fourColorWatershed – four-colour every region in the adjacency graph.
 *
 * Parameters:
 *   adj       – adjacency list produced by buildAdjList().  The region
 *               count is read from adj->region_count.
 *   color_out – caller-allocated array of length (regionCount + 1).
 *               After the call:
 *                 color_out[i] ∈ {0,1,2,3} for each coloured region i.
 *                 color_out[i] == -1 if the region stays uncoloured (only
 *                 possible on failure return).
 *
 * Returns:
 *   1 (COLOR_SUCCESS) – every region is coloured without conflict.
 *   0 (COLOR_FAILURE) – backtracking exhausted or exceeded
 *                       regionCount * 10 steps.
 */
int fourColorWatershed(AdjList *adj, int *color_out);

/*
 * coloringFailureRate – backtrack failure rate of the last call.
 *   failure_rate = backtrack_steps / total_steps  ∈ [0, 1]
 *
 * Used for status reporting.  Returns 0.0 before any call.
 */
double coloringFailureRate(void);

#ifdef __cplusplus
}
#endif

#endif /* COLORING_H */
