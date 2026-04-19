/*
 * seed.cpp  –  Task 1: Poisson-disk seed generation.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  ALGORITHM: Hexagonal Grid + Bounded Random Jitter
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ── Why hexagonal grid + jitter instead of dart throwing ──────────────────
 *
 *  Dart throwing fires random probes across the whole image.  As the image
 *  fills up, the probability of a random probe landing in a free gap drops
 *  rapidly; most probes are wasted.  The algorithm has no analytical
 *  stopping criterion that guarantees a MAXIMAL set without a fudge factor
 *  on d_min.
 *
 *  This implementation uses a deterministic hexagonal lattice as the
 *  "scaffold":
 *    1. Every lattice point is guaranteed to be at distance s > d_min from
 *       all other lattice points (proven below).
 *    2. A small bounded jitter is applied so seeds are not aligned on a
 *       visible regular grid, while the strict distance constraint is
 *       preserved analytically.
 *    3. The total seed count is determined by the grid formula, not by a
 *       random stop condition, so no coefficient is needed to inflate d_min.
 *
 * ── Hexagonal grid spacing formula ────────────────────────────────────────
 *
 *  For a regular hexagonal tiling with centre-to-centre distance s, each
 *  tile has area  A_tile = (√3/2)·s².
 *
 *  We want K tiles to cover M×N pixels:
 *
 *       (√3/2)·s² = M·N / K
 *    →  s² = 2·M·N / (√3·K)
 *    →  s  = sqrt(2·M·N / (√3·K))
 *
 *  Comparing to d_min = sqrt(M·N / K):
 *
 *       s / d_min = sqrt(2/√3) = sqrt(2·√3/3) ≈ 1.0746
 *
 *  Therefore  s > d_min  strictly. ✓
 *
 * ── Proof: minimum pairwise distance after jitter > d_min ─────────────────
 *
 *  Grid layout (standard "flat-row" hex lattice):
 *    • Rows spaced vertically by  row_step = s·√3/2.
 *    • Within each row, points spaced horizontally by  col_step = s.
 *    • Odd-indexed rows are shifted right by  col_step/2  (= s/2).
 *
 *  In this layout every point has exactly 6 nearest neighbours, all at
 *  distance s:
 *    – Same-row neighbour: |Δx| = s, |Δy| = 0  →  dist = s  ✓
 *    – Cross-row neighbour: |Δx| = s/2, |Δy| = s√3/2
 *                           →  dist = √(s²/4 + 3s²/4) = s  ✓
 *    – All other lattice pairs have distance ≥ s·√3 > s.
 *
 *  Jitter bound:
 *       jitter_max = (s − d_min)/2 − ε   (ε = 1e-9)
 *
 *  Let P, Q be any two adjacent lattice points (pre-jitter distance = s).
 *  After jitter, each moves at most jitter_max.  By the triangle inequality:
 *
 *       dist(P', Q') ≥ dist(P, Q) − dist(P, P') − dist(Q, Q')
 *                    ≥ s − jitter_max − jitter_max
 *                    = s − 2·jitter_max
 *                    = s − (s − d_min) + 2ε
 *                    = d_min + 2ε
 *                    > d_min   ✓
 *
 *  For non-adjacent grid point pairs (pre-jitter distance ≥ s·√3):
 *       dist(P', Q') ≥ s·√3 − 2·jitter_max
 *                    = d_min + s·(√3 − 1) + 2ε   (> d_min, since s > 0)  ✓
 *
 *  QED: any two jittered seeds are strictly more than d_min apart.         □
 *
 *  NOTE: integer pixel truncation can reduce distances by at most 1 pixel,
 *  which is captured by the O(n²) validation scan in Step 5.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "seed.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Floating-point candidate point (before integer conversion) ─────────── */
typedef struct { double x, y; } DPoint;

/* ── count_hex_points ────────────────────────────────────────────────────── */
/*
 * Count how many hexagonal lattice points fall strictly inside [0,N)×[0,M)
 * when the grid is laid out with:
 *   col_step = s            (horizontal spacing within each row)
 *   row_step = s·√3/2       (vertical spacing between rows)
 *   odd rows shifted right by s/2
 *
 * Pure counting – no memory allocation.  Mirrors the exact same row/column
 * traversal used in generateSeeds so the two are always in sync.
 */
static int count_hex_points(int M, int N, double s)
{
    double row_step = s * sqrt(3.0) / 2.0;
    int    total    = 0;

    for (int r = 0; ; ++r) {
        double y = (double)r * row_step;
        if (y >= (double)M) break;

        double x_off = (r % 2 == 1) ? s / 2.0 : 0.0;

        for (int c = 0; ; ++c) {
            double x = (double)c * s + x_off;
            if (x >= (double)N) break;
            ++total;
        }
    }
    return total;
}

/* ── Helper: uniform random double in [0, 1) ────────────────────────────── */
static double randDouble(void)
{
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/* ── Helper: clamp integer to [lo, hi] ──────────────────────────────────── */
static int clampI(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ══════════════════════════════════════════════════════════════════════════
   generateSeeds
   ══════════════════════════════════════════════════════════════════════════ */
int generateSeeds(int M, int N, int K, SeedPoint **seeds_out)
{
    /* ── Input validation ───────────────────────────────────────────────── */
    if (M <= 0 || N <= 0 || K <= 0) {
        fprintf(stderr,
                "[ERROR] generateSeeds: M, N, K must all be positive "
                "(got M=%d, N=%d, K=%d).\n", M, N, K);
        return 0;
    }
    if (!seeds_out) {
        fprintf(stderr, "[ERROR] generateSeeds: seeds_out is NULL.\n");
        return 0;
    }

    srand((unsigned int)time(NULL));

    /* ── Step 1: Compute distances and determine grid spacing ─────────── */
    /*
     * d_min   = sqrt(M·N / K)          – minimum required separation (spec).
     * s_init  = sqrt(2·M·N / (√3·K))   – hex lattice spacing (≈ 1.0746·d_min);
     *           produces exactly K tiles in an infinite plane.
     *
     * For finite images, boundary clipping may reduce n_cands slightly below K.
     * The shrink loop corrects this by compressing s by 1 % per step until
     * n_cands ≥ K, stopping at s_floor = d_min·1.001 (jitter_max stays > 0).
     *
     * Integer-truncation discards (Step 5): even when n_cands ≥ K and we take
     * exactly K primary seeds, a handful of adjacent pairs can land at integer
     * distance ≤ d_min after pixel-rounding, leaving final_count < K.
     * Step 6 repairs this by jittering reserve grid points (the n_cands − K
     * "leftover" candidates never selected in Step 3) and inserting each one
     * that passes the conflict check against the already-accepted set.
     * No change to s or jitter_max is needed; the reserve is "free" extra
     * capacity that the hex formula already provides above K.
     */
    double MN       = (double)M * (double)N;
    double d_min    = sqrt(MN / (double)K);
    double d_min_sq = d_min * d_min;

    double s_init  = sqrt(2.0 * MN / (sqrt(3.0) * (double)K));
    double s_floor = d_min * 1.001;
    const double SHRINK = 0.99;

    /*
     * RESERVE: spare grid points beyond K used by Step 6 to replenish seeds
     * that Step 5 discards due to integer-pixel truncation.
     *
     * Design rules:
     *   • RESERVE = K/20 + 10  →  ~5 % of K, at least 10.
     *     Empirical discard rate is ≤ 3 % of K, so 5 % is always ample.
     *   • The extra s-shrink needed is at most a few iterations (s drops
     *     ≤ 5 %) → jitter_max changes negligibly → no cascade effect.
     *   • For small K (100), K/20+10 = 15: only 1–4 shrink iterations;
     *     s stays well above d_min; jitter_max ≈ 0.96 px → no integer-
     *     conflict explosion.
     *   • For large K (1000), K/20+10 = 60: 1–2 shrink iterations;
     *     n_cands jumps from 1003 to ~1068; reserve = ~68 → absorbs the
     *     ~26 integer-truncation discards observed for this K.
     */
    const int RESERVE    = K / 20 + 10;
    const int need_cands = K + RESERVE;

    double s = s_init;
    while (count_hex_points(M, N, s) < need_cands && s > s_floor)
        s *= SHRINK;

    if (count_hex_points(M, N, s) < K) {
        printf("  [警告] 图像尺寸 %d×%d 在最小距离约束下物理上无法容纳 %d 个种子点，\n"
               "         将返回实际可生成数量。\n", N, M, K);
    }

    double col_step   = s;
    double row_step   = s * sqrt(3.0) / 2.0;
    double jitter_max = (s - d_min) / 2.0 - 1e-9;

    /* ── Step 2: Generate hex lattice candidate centres ───────────────── */
    /*
     * Conservative upper bound on candidate count:
     *   rows: ceil(M / row_step) + 1 ≈ K · row_step·col_step / MN + margin
     *   cols: ceil(N / col_step) + 1
     * In practice the product is close to K; add generous margin.
     */
    int n_rows_max = (int)(MN / (row_step * col_step)) + 128;
    if (n_rows_max < K + 128) n_rows_max = K + 128;

    DPoint *cands  = (DPoint *)safeMalloc((size_t)n_rows_max * sizeof(DPoint));
    int     n_cands = 0;

    for (int r = 0; ; ++r) {
        double y = (double)r * row_step;
        if (y >= (double)M) break;                    /* row out of image */

        /* Odd rows are shifted right by half a column step */
        double x_off = (r % 2 == 1) ? col_step / 2.0 : 0.0;

        for (int c = 0; ; ++c) {
            double x = (double)c * col_step + x_off;
            if (x >= (double)N) break;                /* column out of image */

            /* Grow array if needed (rare – initial estimate is generous) */
            if (n_cands >= n_rows_max) {
                n_rows_max *= 2;
                cands = (DPoint *)realloc(cands,
                                          (size_t)n_rows_max * sizeof(DPoint));
                if (!cands) {
                    fprintf(stderr, "[ERROR] realloc failed in generateSeeds\n");
                    exit(1);
                }
            }
            cands[n_cands].x = x;
            cands[n_cands].y = y;
            ++n_cands;
        }
    }

    /* ── Step 3: Full shuffle; primary = first min(n_cands, K) ──────── */
    /*
     * Full Fisher-Yates over ALL n_cands so the candidates beyond the first K
     * ("reserve") are a random subset of the remaining grid points.  Step 6
     * pulls from this reserve to fill any gap left by Step 5 discards.
     */
    for (int i = 0; i < n_cands; ++i) {
        int span = n_cands - i;
        int j    = i + (int)(randDouble() * (double)span);
        if (j >= n_cands) j = n_cands - 1;

        DPoint tmp = cands[i];
        cands[i]   = cands[j];
        cands[j]   = tmp;
    }
    int n_primary = (n_cands < K) ? n_cands : K;   /* primary batch size */

    /* ── Step 4: Apply bounded random jitter ──────────────────────────── */
    /*
     * Each candidate centre (cx, cy) is displaced by a vector with:
     *   • direction: angle ~ Uniform[0, 2π)
     *   • magnitude: r = jitter_max · √(u),  u ~ Uniform[0, 1)
     *     The √ transform yields a point distributed UNIFORMLY WITHIN the
     *     disk of radius jitter_max (area-uniform sampling).
     *
     * The maximum displacement is strictly < jitter_max (since u < 1),
     * which is consistent with the distance proof above.
     *
     * Coordinates are clamped to [0, N-1] × [0, M-1] so seeds are always
     * valid pixel addresses (clamping only reduces displacement → safer).
     */
    /* Allocate K slots – the most we ever return. */
    SeedPoint *seeds = (SeedPoint *)safeMalloc((size_t)K * sizeof(SeedPoint));

    for (int i = 0; i < n_primary; ++i) {
        double cx  = cands[i].x;
        double cy  = cands[i].y;
        double ang = randDouble() * 2.0 * M_PI;
        double r   = jitter_max * sqrt(randDouble());   /* area-uniform */

        int fx = clampI((int)(cx + r * cos(ang)), 0, N - 1);
        int fy = clampI((int)(cy + r * sin(ang)), 0, M - 1);

        seeds[i].x  = fx;
        seeds[i].y  = fy;
        seeds[i].id = i + 1;   /* 1-based, reassigned after compaction */
    }
    int count = n_primary;

    /* cands is kept alive until after Step 6 (reserve supplementation). */

    /* ── Step 5: O(n²) validation – discard points violating dist > d_min */
    /*
     * Theory guarantees no violations; this scan is a safety net against:
     *   (a) integer truncation reducing a borderline pair below d_min, or
     *   (b) boundary clamping pushing two distinct centres to the same pixel.
     * For the violating pair (i, j), we keep i and discard j (first-wins).
     */
    int *keep      = (int *)safeMalloc((size_t)count * sizeof(int));
    int  n_discard = 0;

    for (int i = 0; i < count; ++i) keep[i] = 1;

    for (int i = 0; i < count; ++i) {
        if (!keep[i]) continue;
        for (int j = i + 1; j < count; ++j) {
            if (!keep[j]) continue;
            double dx = (double)(seeds[i].x - seeds[j].x);
            double dy = (double)(seeds[i].y - seeds[j].y);
            /* Discard j if dist(i,j) ≤ d_min (violates strict constraint) */
            if (dx * dx + dy * dy <= d_min_sq) {
                keep[j] = 0;
                ++n_discard;
            }
        }
    }

    if (n_discard > 0)
        printf("  [验证] 整数截断修正：丢弃 %d 个距离不足的种子点\n", n_discard);

    /* In-place compaction + re-number IDs */
    int final_count = 0;
    for (int i = 0; i < count; ++i) {
        if (!keep[i]) continue;
        seeds[final_count]    = seeds[i];
        seeds[final_count].id = final_count + 1;
        ++final_count;
    }
    free(keep);

    /* ── Step 6: Supplement from reserve to reach exactly K ─────────── */
    /*
     * Reserve = cands[n_primary .. n_cands-1] (randomised by the full
     * Fisher-Yates in Step 3).  Each reserve candidate is jittered and
     * conflict-checked against the already-accepted seeds in O(final_count).
     * Because all grid points are pairwise ≥ s > d_min apart (pre-jitter),
     * reserve-to-reserve and reserve-to-primary conflicts are rare; they
     * arise only from integer truncation, just like Step 5 discards.
     */
    if (final_count < K) {
        int n_added = 0;
        for (int i = n_primary; i < n_cands && final_count < K; ++i) {
            double cx  = cands[i].x;
            double cy  = cands[i].y;
            double ang = randDouble() * 2.0 * M_PI;
            double r   = jitter_max * sqrt(randDouble());
            int    fx  = clampI((int)(cx + r * cos(ang)), 0, N - 1);
            int    fy  = clampI((int)(cy + r * sin(ang)), 0, M - 1);

            /* Conflict-check against all already-accepted seeds */
            int conflict = 0;
            for (int j = 0; j < final_count && !conflict; ++j) {
                double dx = (double)(fx - seeds[j].x);
                double dy = (double)(fy - seeds[j].y);
                if (dx * dx + dy * dy <= d_min_sq) conflict = 1;
            }
            if (!conflict) {
                seeds[final_count].x  = fx;
                seeds[final_count].y  = fy;
                seeds[final_count].id = final_count + 1;
                ++final_count;
                ++n_added;
            }
        }
        if (n_added > 0)
            printf("  [补充] 从预留格点补入 %d 个种子点\n", n_added);
    }

    free(cands);
    cands = NULL;

    /* ── Report if still fewer than K after reserve exhausted ───────── */
    if (final_count < K) {
        printf("  [提示] 六边形网格采样实际生成 %d 个种子点，请求 K = %d\n"
               "         （图像物理尺寸限制，调用方已知晓）\n",
               final_count, K);
    }

    *seeds_out = seeds;
    return final_count;
}

/* ── freeSeeds ───────────────────────────────────────────────────────────── */

void freeSeeds(SeedPoint *seeds)
{
    free(seeds);
}

/* ── Optional self-test (compile with -DTEST_SEED) ──────────────────────── */
/*
 * Compile & run (from the project directory):
 *   g++ -std=c++17 -DTEST_SEED seed.cpp utils.cpp -o /tmp/test_seed -lm \
 *       && /tmp/test_seed
 */
#ifdef TEST_SEED

int main(void)
{
    const int M = 600, N = 600;
    const int cases[]   = {100, 500, 1000};
    const int num_cases = (int)(sizeof cases / sizeof cases[0]);

    printf("六边形网格 + 有界抖动 Poisson-disk 自测  (%d × %d)\n\n", M, N);
    printf("%-6s  %-8s  %-8s  %-10s  %-9s  %s\n",
           "K", "seeds", "ratio", "time(ms)", "jmax(px)", "丢弃点");
    printf("────────────────────────────────────────────────────────\n");

    for (int i = 0; i < num_cases; ++i) {
        int K = cases[i];

        /* Compute expected parameters */
        double d_min = sqrt((double)(M * N) / K);
        double s     = sqrt(2.0 * M * N / (sqrt(3.0) * K));
        double jmax  = (s - d_min) / 2.0 - 1e-9;

        SeedPoint *seeds = NULL;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        int cnt = generateSeeds(M, N, K, &seeds);
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double ms = (t1.tv_sec - t0.tv_sec) * 1e3
                  + (t1.tv_nsec - t0.tv_nsec) * 1e-6;

        printf("%-6d  %-8d  %-8.3f  %-10.3f  %-9.4f\n",
               K, cnt, (double)cnt / K, ms, jmax);

        freeSeeds(seeds);
    }
    return 0;
}

#endif /* TEST_SEED */
