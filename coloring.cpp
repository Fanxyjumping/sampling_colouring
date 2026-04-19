/*
 * coloring.cpp
 * Task 2 – Four-colour graph colouring.
 *
 * ── Algorithm ──────────────────────────────────────────────────────────
 *
 * Simple chronological backtracking (BFS order) is exponential on dense
 * planar graphs: when a region has 4 differently-coloured neighbours the
 * backtracker thrashes between unrelated nodes without ever resolving the
 * conflict, hitting any finite limit.
 *
 * The correct polynomial approach:
 *
 *   Phase 1 – DSatur greedy
 *     At each step pick the most-constrained uncoloured region (highest
 *     saturation = most distinct neighbour colours, tie-break on degree).
 *     Assign the lowest colour in {0,1,2,3} not used by a neighbour.
 *     If all 4 colours are blocked, defer the region for Phase 2.
 *     The assignment sequence is recorded on an explicit StackNode list.
 *
 *   Phase 2 – Kempe chain repair
 *     For each deferred region v, try every pair (c1,c2) from {0..3}.
 *     BFS in the subgraph induced by {c1,c2}-coloured nodes, starting
 *     from v's c1-coloured neighbour.  If none of v's c2-coloured
 *     neighbours lie in the same component, swap c1↔c2 within that
 *     component: the swap is conflict-free and frees colour c1 for v.
 *     By the 5-colour theorem this always succeeds for planar graphs
 *     (watershed adjacency graphs are always planar).
 *
 * ── Data structures ────────────────────────────────────────────────────
 *   BFS queue     : singly-linked QueueNode (Kempe BFS)
 *   Stack         : singly-linked StackNode (Phase-1 assignment sequence)
 *   Adjacency list: AdjList / AdjNode (from adjacency.h)
 */

#include "coloring.h"
#include "adjacency.h"
#include "utils.h"
#include <cstdlib>
#include <cstring>

/* ── Internal linked-list node types ───────────────────────────────── */

typedef struct QueueNode {
    int               id;
    struct QueueNode *next;
} QueueNode;

typedef struct StackNode {
    int               region_id;
    int               color;
    int               order_index;
    struct StackNode *next;
} StackNode;

/* ── Module-level statistics ────────────────────────────────────────── */

static double g_failure_rate = 0.0;

/* ── findAvailableColor ─────────────────────────────────────────────── */
/*
 * Return the lowest colour in [startFrom, 3] not used by any coloured
 * neighbour of regionId.  Returns -1 if none available.
 * Neighbours with color < 0 (uncoloured / deferred) are ignored.
 */
static int findAvailableColor(int regionId, int *color,
                              AdjList *adj, int startFrom)
{
    unsigned char used[4] = {0, 0, 0, 0};
    for (AdjNode *nb = adj->lists[regionId]; nb; nb = nb->next) {
        int c = color[nb->neighbor_id];
        if (c >= 0 && c < 4) used[c] = 1;
    }
    for (int c = startFrom; c < 4; ++c)
        if (!used[c]) return c;
    return -1;
}

/* ── kempe_repair ───────────────────────────────────────────────────── */
/*
 * Try to free a colour slot for region v by performing a Kempe chain
 * swap.  Tries all 6 colour pairs from {0,1,2,3}.
 *
 * Returns 1 and sets color_out[v] on success, 0 if all pairs fail
 * (should not happen for planar graphs).
 */
static int kempe_repair(int v, int *color_out, AdjList *adj)
{
    int n = adj->region_count;

    /* Fast path: maybe a colour is already free (saturation < 4) */
    int c = findAvailableColor(v, color_out, adj, 0);
    if (c != -1) { color_out[v] = c; return 1; }

    /* All 4 colours are used by neighbours → try each of the 6 pairs */
    for (int c1 = 0; c1 < 4; ++c1) {
        for (int c2 = c1 + 1; c2 < 4; ++c2) {

            /* Find the c1-coloured neighbour that starts the Kempe chain */
            int start = -1;
            for (AdjNode *nb = adj->lists[v]; nb; nb = nb->next)
                if (color_out[nb->neighbor_id] == c1)
                    { start = nb->neighbor_id; break; }
            if (start == -1) continue;   /* c1 not actually used */

            /* BFS in the {c1,c2}-induced subgraph from start */
            unsigned char *vis =
                (unsigned char *)safeCalloc((size_t)(n + 1), 1);
            int *q  = (int *)safeMalloc((size_t)(n + 1) * sizeof(int));
            int qh  = 0, qt = 0;
            vis[start] = 1;
            q[qt++]    = start;
            while (qh < qt) {
                int cur = q[qh++];
                for (AdjNode *nb = adj->lists[cur]; nb; nb = nb->next) {
                    int nid = nb->neighbor_id;
                    if (!vis[nid] &&
                        (color_out[nid] == c1 || color_out[nid] == c2)) {
                        vis[nid] = 1;
                        q[qt++]  = nid;
                    }
                }
            }
            free(q);

            /* Is any c2-coloured neighbour of v in the same component? */
            int c2_reachable = 0;
            for (AdjNode *nb = adj->lists[v]; nb; nb = nb->next)
                if (color_out[nb->neighbor_id] == c2 && vis[nb->neighbor_id])
                    { c2_reachable = 1; break; }

            if (!c2_reachable) {
                /* Safe to swap: c1 and c2 swap roles in the chain,
                   which frees colour c1 for v without new conflicts. */
                for (int id = 1; id <= n; ++id)
                    if (vis[id])
                        color_out[id] = (color_out[id] == c1) ? c2 : c1;
                color_out[v] = c1;
                free(vis);
                return 1;
            }
            free(vis);
        }
    }
    return 0;   /* all 6 pairs failed – shouldn't occur for planar graphs */
}

/* ── fourColorWatershed ─────────────────────────────────────────────── */

int fourColorWatershed(AdjList *adj, int *color_out)
{
    /* ── Input validation ──────────────────────────────────────────── */
    if (!adj || !color_out) {
        g_failure_rate = 0.0;
        return COLOR_FAILURE;
    }
    int region_count = adj->region_count;
    if (region_count <= 0) {
        g_failure_rate = 0.0;
        return COLOR_FAILURE;
    }

    /* colour_out[0] unused; init all to -1 (uncoloured) */
    memset(color_out, -1, (size_t)(region_count + 1) * sizeof(int));

    StackNode *stack_top   = NULL;
    long long  forward_ok  = 0;   /* regions coloured directly by greedy */
    long long  repair_cnt  = 0;   /* regions that needed Kempe repair     */
    int        status      = COLOR_SUCCESS;

    /* ── Phase 1: DSatur greedy ────────────────────────────────────── */
    /*
     * colour_out values during Phase 1:
     *   -1  : uncoloured, available for selection
     *   0-3 : coloured by greedy
     *   -2  : deferred (all 4 colours blocked); handled in Phase 2
     *
     * -2 != -1, so deferred regions are skipped by the "!= -1" guard
     * and their absence from the coloured set does not affect saturation
     * calculations for their neighbours (findAvailableColor ignores c < 0).
     */
    int processed = 0;
    while (processed < region_count) {

        /* Pick the most-constrained uncoloured region */
        int best_rid = -1, best_sat = -1, best_deg = -1;
        for (int id = 1; id <= region_count; ++id) {
            if (color_out[id] != -1) continue;   /* coloured or deferred */

            unsigned char seen[4] = {0, 0, 0, 0};
            int deg = 0;
            for (AdjNode *nb = adj->lists[id]; nb; nb = nb->next) {
                int c = color_out[nb->neighbor_id];
                if (c >= 0 && c < 4) seen[c] = 1;
                ++deg;
            }
            int sat = seen[0] + seen[1] + seen[2] + seen[3];
            if (sat > best_sat || (sat == best_sat && deg > best_deg)) {
                best_rid = id;
                best_sat = sat;
                best_deg = deg;
            }
        }
        if (best_rid == -1) break;   /* all regions handled */

        int c = findAvailableColor(best_rid, color_out, adj, 0);

        if (c != -1) {
            /* Direct greedy assignment */
            color_out[best_rid] = c;

            StackNode *sn   = (StackNode *)safeMalloc(sizeof(StackNode));
            sn->region_id   = best_rid;
            sn->color       = c;
            sn->order_index = (int)forward_ok;
            sn->next        = stack_top;
            stack_top       = sn;

            ++forward_ok;
        } else {
            /* All 4 colours blocked – defer to Kempe repair */
            color_out[best_rid] = -2;
            ++repair_cnt;
        }
        ++processed;
    }

    /* ── Phase 2: Kempe chain repair (iterative) ─────────────────── */
    /*
     * Deferred nodes may be adjacent to each other.  If we process them
     * in a single pass, an earlier repair might block a later one.
     * Iterative approach: each round processes every still-deferred (-2)
     * node.  On success the node is coloured; on failure it stays -2 for
     * the next round.  Each round makes at least one node, so the loop
     * terminates in at most repair_cnt rounds.
     */
    {
        int pending = (int)repair_cnt;
        while (pending > 0) {
            int resolved = 0;
            for (int id = 1; id <= region_count; ++id) {
                if (color_out[id] != -2) continue;
                color_out[id] = -1;         /* expose for repair */
                if (kempe_repair(id, color_out, adj)) {
                    ++resolved;
                } else {
                    color_out[id] = -2;     /* restore, retry next round */
                }
            }
            if (resolved == 0) {
                /* No progress – force-assign remaining nodes.
                   Should not happen for a planar adjacency graph.       */
                for (int id = 1; id <= region_count; ++id) {
                    if (color_out[id] == -2) {
                        color_out[id] = 0;
                        status = COLOR_FAILURE;
                    }
                }
                break;
            }
            pending -= resolved;
        }
    }

    /* ── Clean up stack ───────────────────────────────────────────── */
    while (stack_top) {
        StackNode *tmp = stack_top;
        stack_top      = tmp->next;
        free(tmp);
    }

    /* ── Failure rate: repairs / direct colorings ─────────────────── */
    g_failure_rate = (forward_ok > 0)
        ? (double)repair_cnt / (double)forward_ok
        : (status == COLOR_SUCCESS ? 0.0 : 1.0);

    return status;
}

/* ── coloringFailureRate ────────────────────────────────────────────── */

double coloringFailureRate(void)
{
    return g_failure_rate;
}
