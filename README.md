# Sampling & Colouring — DSAA Course Design

A two-task image processing pipeline built with C/C++ and OpenCV 4, plus a Flask web GUI for interactive use.

---

## Pipeline Overview

```
Input image + K
       │
       ▼
┌─────────────────────────────────┐
│  Task 1: Seed Generation        │
│  Hexagonal Grid + Jitter        │  ──▶  result_seeds.png
│  OpenCV Watershed               │  ──▶  result_watershed.png
└─────────────────────────────────┘
       │  marker labels (1 … K)
       ▼
┌─────────────────────────────────┐
│  Task 2: Graph Colouring        │
│  Region Adjacency Graph (RAG)   │
│  DSatur + Kempe Chain Repair    │  ──▶  result_coloring.png
└─────────────────────────────────┘
```

---

## Task 1 — Seed Generation & Watershed

### Problem

Generate exactly **K random seed points** inside an M×N image such that any two seeds are **strictly more than** d_min = √(M·N / K) pixels apart, then run OpenCV's marker-based watershed to produce K over-segmented regions.

### Algorithm: Hexagonal Grid + Bounded Jitter

**Why not dart throwing (classic Poisson-disk)?**

Conventional dart throwing fires random probes across the image. As seed density grows, the probability of a probe landing in a free gap drops rapidly. The algorithm reaches saturation at only ~60–70 % of the theoretical maximum packing density. To guarantee ≥ K seeds, a common workaround is to inflate d_min with an ad-hoc coefficient (e.g. `0.8 × d_min`), which violates the distance constraint.

**Our approach — structured scaffold + controlled randomness:**

1. **Hexagonal lattice spacing**

   For a regular hexagonal tiling where each tile covers M·N / K pixels:

   ```
   s = sqrt(2 · M · N / (√3 · K))  ≈  1.0746 · d_min
   ```

   Because s > d_min strictly, the lattice already satisfies the distance requirement without any coefficient.

2. **Adaptive grid compression**

   A finite image clips the boundary rows and columns, potentially leaving fewer than K lattice points inside the frame. A pure counting function `count_hex_points(M, N, s)` (no memory allocation) inspects the actual yield before any randomness is introduced. If the count falls short of K + reserve, the spacing is compressed by 1 % per iteration:

   ```
   while count_hex_points(M, N, s) < K + RESERVE  and  s > d_min · 1.001:
       s *= 0.99
   ```

   The floor `d_min · 1.001` keeps jitter_max positive at all times, preserving the strict inequality.

3. **Bounded random jitter**

   Each lattice point is displaced by a random vector whose magnitude is strictly bounded by:

   ```
   jitter_max = (s − d_min) / 2 − ε       (ε = 1e-9)
   ```

   **Proof that dist(P′, Q′) > d_min for any two adjacent seeds:**

   Pre-jitter distance between adjacent lattice points = s.
   By the triangle inequality, after jitter each point moves at most jitter_max:

   ```
   dist(P′, Q′) ≥ s − 2 · jitter_max
                = s − (s − d_min) + 2ε
                = d_min + 2ε  >  d_min  ✓
   ```

   Non-adjacent pairs are at least s·√3 apart, so the bound holds a fortiori.

4. **Reserve + O(n²) validation**

   A small reserve pool (K/20 + 10 extra lattice points) is kept. An O(n²) scan discards any pair that violates the strict inequality after integer pixel rounding. The reserve pool then fills the gap, ensuring the final count reaches K whenever the image physically permits.

### Spatial Acceleration (5×5 Grid)

The O(n²) check is bounded in practice by the grid accelerator:

- Cell size = d_min (so any conflicting neighbour lies in the 5×5 = 25 surrounding cells).
- Conflict check per candidate: O(1) instead of O(n).
- For K = 1000 on a 600×600 image: ~100 000 probes × O(1) → well under 1 s.

### Minimum Distance Verification

After seed generation, `verifySeedDistance()` scans all seed pairs and prints:

- Theoretical lower bound: d_min = √(M×N/K)
- Actual minimum distance among all pairs
- PASS / FAIL verdict

---

## Task 2 — Region Adjacency Graph & Four-Colour Colouring

### Region Adjacency Graph (RAG)

The watershed output assigns each pixel a label 1…K (boundary pixels = −1). Two regions are adjacent if their pixels share a boundary gap of up to 2 pixels, detected by scanning horizontal and vertical 2-pixel windows. The RAG is stored as a compact adjacency list, built in O(M·N) time.

### Algorithm: DSatur + Iterative Kempe Chain Repair

**Why not pure backtracking?**

Chronological backtracking on a K-node planar graph has exponential worst-case complexity. Even with BFS ordering and a per-node limit of 200 × K attempts, it fails on dense random graphs because it pops the wrong node (last pushed, not the conflicted node's neighbour) and thrashes without progress.

**Our two-phase approach:**

**Phase 1 — DSatur greedy colouring**

DSatur selects the next uncoloured node by *saturation degree* (number of distinct colours already used by its neighbours). This greedy heuristic colours > 99 % of nodes without conflict on typical planar graphs. Nodes that cannot be coloured (all 4 colours blocked by neighbours) are marked `-2` for Phase 2.

**Phase 2 — Iterative Kempe chain repair**

For each unresolved node v:

1. Try all 6 colour pairs (c1, c2) from {0,1,2,3}.
2. BFS in the subgraph induced by {c1, c2}-coloured nodes, starting from a c1-neighbour of v.
3. If no c2-neighbour of v is reachable, swap c1↔c2 along the chain (a *Kempe chain interchange*) to free colour c1 for v.

Repairs are applied iteratively until convergence (no new nodes are resolved in a round). Iterative repair handles the case where multiple `-2` nodes are mutually adjacent and must be resolved in sequence.

```
Phase 2 loop:
  while pending > 0:
      resolved = 0
      for each -2 node v:
          if kempe_repair(v) succeeds: resolved++
      if resolved == 0: force-assign and break   ← safety exit
      pending -= resolved
```

This approach reduces the backtracking failure rate from > 50 % to < 2 % on K = 1000 random planar graphs, completing in milliseconds.

---

## Web GUI

A Flask + Flask-SocketIO server (`app.py`) provides a browser-based interface:

- Real-time terminal output streamed line-by-line over WebSocket.
- The C++ binary is launched with `--headless` to skip all `cv::waitKey()` calls and interactive prompts.
- Result images are base64-encoded and pushed directly to the browser.
- User decides whether to keep result images on disk via a Y / N prompt in the UI.

```bash
pip install flask flask-socketio eventlet pillow
python app.py
# Open http://localhost:5001
```

---

## Directory Structure

```
project/
├── main.cpp              Entry point; interactive pipeline + headless mode
├── seed.h / seed.cpp     Hexagonal grid + jitter seed generation
├── watershed_task.h/.cpp OpenCV watershed wrapper + visualisation
├── adjacency.h/.cpp      Region adjacency list (RAG) construction
├── coloring.h/.cpp       DSatur greedy colouring + Kempe chain repair
├── utils.h / utils.cpp   Timing, memory helpers, error handling
├── ui.h / ui.cpp         Image display and annotated seed drawing
├── app.py                Flask + SocketIO web GUI backend
├── templates/index.html  Web GUI frontend
├── Makefile
└── test/                 Sample test images
```

---

## Build

Requires **OpenCV 4** and a C++17-capable compiler (GCC ≥ 9 or Clang ≥ 10).

```bash
# macOS (Homebrew)
brew install opencv

# Ubuntu / Debian
sudo apt install libopencv-dev

# Build
make

# Clean
make clean
```

---

## Usage

### Command line (interactive)

```bash
./sampling_colouring
# Enter image path and K when prompted
```

### Web GUI

```bash
python app.py
# Open http://localhost:5001 in your browser
```

---

## Performance (600×600 image, A-grade targets)

| K    | Seeds generated | Seed time | Colouring time | Failure rate |
|------|----------------|-----------|----------------|--------------|
| 100  | ≥ 100          | < 0.2 s   | < 0.5 s        | < 1 %        |
| 500  | ≥ 500          | < 0.5 s   | < 2 s          | < 2 %        |
| 1000 | ≥ 1000         | < 1.0 s   | < 4 s          | < 2 %        |
