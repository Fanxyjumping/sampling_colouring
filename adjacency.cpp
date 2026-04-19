/*
 * adjacency.cpp
 * Task 2 – Region adjacency list (RAG) construction.
 *
 * ── Algorithm ──────────────────────────────────────────────────────────
 *  Single O(M×N) scan:
 *    For every pixel P(r,c) with a valid region ID a (>0):
 *      - Check right  neighbour P(r, c+1) with ID b.
 *      - Check bottom neighbour P(r+1, c) with ID b.
 *      If b > 0 and b ≠ a, mark adj_flag[a][b] = adj_flag[b][a] = true.
 *
 *  Deduplication:
 *    A flat heap-allocated bool matrix adj_flag[(regionCount+1)²] records
 *    which pairs have already been seen.  Each write is O(1), so the total
 *    scan remains O(M×N).  For regionCount=1000 the matrix is ~1 MB.
 *
 *  List construction:
 *    After the scan, convert adj_flag into singly-linked AdjNode lists
 *    (one list per region), then free adj_flag.
 */

#include "adjacency.h"
#include "utils.h"
#include <cstdlib>
#include <cstring>

/* ── buildAdjList ────────────────────────────────────────────────── */

AdjList *buildAdjList(cv::Mat &markers, int regionCount) {

    /* ── Input validation ──────────────────────────────────────────── */
    if (markers.empty()) {
        warnMsg("buildAdjList: markers matrix is empty.");
        return NULL;
    }
    if (regionCount <= 0) {
        warnMsg("buildAdjList: regionCount must be > 0 (got %d).", regionCount);
        return NULL;
    }

    /* ── Step 1: Allocate temporary deduplication matrix ─────────────
     *  adj_flag is a flat (regionCount+1)×(regionCount+1) bool matrix.
     *  adj_flag[a*(n1)+b] == 1 means regions a and b are already recorded
     *  as adjacent.  Index 0 is unused (region IDs start at 1).        */
    int n1 = regionCount + 1;
    unsigned char *adj_flag =
        (unsigned char *)safeCalloc((size_t)n1 * (size_t)n1, 1);

    /* ── Step 2: Single O(M×N) scan – check right and bottom neighbours */
    for (int r = 0; r < markers.rows; ++r) {
        const int *row = markers.ptr<int>(r);
        for (int c = 0; c < markers.cols; ++c) {
            int a = row[c];
            /* Skip boundary pixels (-1) and anything outside valid range */
            if (a <= 0 || a > regionCount) continue;

            /* Right neighbour P(r, c+1) – direct */
            if (c + 1 < markers.cols) {
                int b = row[c + 1];
                if (b > 0 && b <= regionCount && b != a) {
                    adj_flag[a * n1 + b] = 1;
                    adj_flag[b * n1 + a] = 1;
                }
            }
            /* Right neighbour P(r, c+2) through a boundary pixel (-1) */
            if (c + 2 < markers.cols && row[c + 1] == -1) {
                int b = row[c + 2];
                if (b > 0 && b <= regionCount && b != a) {
                    adj_flag[a * n1 + b] = 1;
                    adj_flag[b * n1 + a] = 1;
                }
            }

            /* Bottom neighbour P(r+1, c) – direct */
            if (r + 1 < markers.rows) {
                int b = markers.at<int>(r + 1, c);
                if (b > 0 && b <= regionCount && b != a) {
                    adj_flag[a * n1 + b] = 1;
                    adj_flag[b * n1 + a] = 1;
                }
            }
            /* Bottom neighbour P(r+2, c) through a boundary pixel (-1) */
            if (r + 2 < markers.rows && markers.at<int>(r + 1, c) == -1) {
                int b = markers.at<int>(r + 2, c);
                if (b > 0 && b <= regionCount && b != a) {
                    adj_flag[a * n1 + b] = 1;
                    adj_flag[b * n1 + a] = 1;
                }
            }
        }
    }

    /* ── Step 3: Build singly-linked adjacency lists from adj_flag ───
     *  For region i, walk column j=1..regionCount; when adj_flag[i][j]==1
     *  prepend a new AdjNode to lists[i].  Prepending is O(1) per node.  */
    AdjList *adj = (AdjList *)safeMalloc(sizeof(AdjList));
    adj->region_count = regionCount;
    /* lists[0] unused; allocate n1 slots so indexing is simply lists[i] */
    adj->lists = (AdjNode **)safeCalloc((size_t)n1, sizeof(AdjNode *));

    for (int i = 1; i <= regionCount; ++i) {
        for (int j = 1; j <= regionCount; ++j) {
            if (adj_flag[i * n1 + j]) {
                AdjNode *node     = (AdjNode *)safeMalloc(sizeof(AdjNode));
                node->neighbor_id = j;
                node->next        = adj->lists[i]; /* prepend */
                adj->lists[i]     = node;
            }
        }
    }

    /* ── Step 4: Free the temporary matrix ──────────────────────────── */
    free(adj_flag);

    return adj;
}

/* ── freeAdjList ─────────────────────────────────────────────────── */

void freeAdjList(AdjList *adj) {
    if (!adj) return;

    /* Free every node in every neighbour list */
    for (int i = 0; i <= adj->region_count; ++i) {
        AdjNode *cur = adj->lists[i];
        while (cur) {
            AdjNode *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(adj->lists);
    free(adj);
}

/* ── getNeighborCount ────────────────────────────────────────────── */

int getNeighborCount(const AdjList *adj, int regionId) {
    if (!adj || regionId <= 0 || regionId > adj->region_count) return 0;

    int count = 0;
    for (AdjNode *n = adj->lists[regionId]; n; n = n->next)
        ++count;
    return count;
}
