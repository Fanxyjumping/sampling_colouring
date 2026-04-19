# Sampling & Colouring — DSAA Course Design

## Overview

Two-task image processing pipeline built with C/C++ and OpenCV 4:

| Task | Description |
|------|-------------|
| Task 1 | Poisson-disk seed generation → OpenCV `watershed` over-segmentation |
| Task 2 | Region Adjacency Graph construction → BFS-ordered four-colour backtracking |

## Directory Structure

```
project/
├── main.cpp              Entry point, interactive pipeline
├── seed.h / seed.cpp     Poisson-disk sampling with grid acceleration
├── watershed_task.h/.cpp OpenCV watershed wrapper + visualisation
├── adjacency.h/.cpp      Region adjacency list (RAG) construction
├── coloring.h/.cpp       BFS four-colour colouring + backtracking
├── utils.h / utils.cpp   Timing, error handling, memory helpers
├── ui.h / ui.cpp         Image display and status output
├── Makefile
└── test/                 Test images and output results
```

## Build

Requires **OpenCV 4** and a C++17-capable compiler (GCC ≥ 9 or Clang ≥ 10).

```bash
# macOS (Homebrew)
brew install opencv

# Ubuntu/Debian
sudo apt install libopencv-dev

# Build
make

# Debug build
make debug

# Clean
make clean
```

## Usage

```bash
# With arguments
./sampling_colouring <image_path> <K>

# Interactive mode
./sampling_colouring
```

## Acceptance Criteria (Grade A)

### Task 1 — Seed Generation + Watershed
| K    | Seeds produced | Time  |
|------|---------------|-------|
| 1000 | > 1000        | < 1 s |
| 500  | > 500         | < 0.5 s |
| 100  | > 100         | < 0.2 s |

### Task 2 — Four-Colour Colouring
| K    | Backtrack fail rate | Time   |
|------|---------------------|--------|
| 1000 | < 40%              | < 4 s  |
| 500  | < 20%              | < 2 s  |
| 100  | < 10%              | < 0.5 s |
