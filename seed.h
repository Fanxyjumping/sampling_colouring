/*
 * seed.h
 * Task 1 – Seed point generation via Poisson-disk sampling with spatial
 * grid acceleration.
 *
 * Guarantees that any two generated seed points are separated by more than
 *   d_min = sqrt((double)(M * N) / K)  pixels.
 */

#ifndef SEED_H
#define SEED_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A single seed point: pixel coordinate (x = column, y = row) plus a
 * 1-based integer ID used as the watershed marker label.
 */
typedef struct {
    int x;   /* column index, 0-based */
    int y;   /* row    index, 0-based */
    int id;  /* 1-based region label  */
} SeedPoint;

/*
 * generateSeeds – generate seed points satisfying the minimum-distance
 * constraint using Poisson-disk sampling with grid acceleration.
 *
 * Parameters:
 *   M         – image height in pixels (must be > 0)
 *   N         – image width  in pixels (must be > 0)
 *   K         – requested seed count   (must be > 0)
 *   seeds_out – output pointer; on success *seeds_out points to a
 *               heap-allocated SeedPoint array.  The caller is responsible
 *               for calling free(*seeds_out) when done.
 *
 * Returns:
 *   The actual number of seeds generated (≥ K is the A-grade target).
 *   Returns 0 on invalid input; *seeds_out is left untouched in that case.
 *
 * Performance targets (A-grade, 600×600 image):
 *   K=1000 → > 1000 seeds, < 1.0 s
 *   K=500  → > 500  seeds, < 0.5 s
 *   K=100  → > 100  seeds, < 0.2 s
 */
int generateSeeds(int M, int N, int K, SeedPoint **seeds_out);

/*
 * freeSeeds – release a SeedPoint array returned by generateSeeds().
 */
void freeSeeds(SeedPoint *seeds);

#ifdef __cplusplus
}
#endif

#endif /* SEED_H */
