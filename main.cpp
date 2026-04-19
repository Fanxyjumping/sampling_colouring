/*
 * main.cpp
 * Entry point: interactive pipeline integrating all modules.
 *
 * Flow:
 *   1. Prompt image path (retry on failure).
 *   2. Prompt K (retry on invalid input).
 *   3. Task 1 – seed generation + watershed (with timing).
 *      Display: ① seed annotation window  ② watershed blend window.
 *   4. "Press any key to continue Task 2…" → waitKey().
 *   5. Task 2 – adjacency list + four-colour colouring (with timing).
 *      Display four-colour result on success; print reason on failure.
 *   6. Ask whether to save the three result images.
 *   7. Free all dynamic memory, exit normally.
 *
 * Compile & run:
 *   make && ./sampling_colouring
 */

#include "seed.h"
#include "watershed_task.h"
#include "adjacency.h"
#include "coloring.h"
#include "ui.h"
#include "utils.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

/* ── Minimum-distance verification ─────────────────────────────────── */
/*
 * verifySeedDistance – scan all seed pairs and print:
 *   • theoretical d_min  (the guarantee from the sampler)
 *   • actual minimum distance between any two seeds
 *   • PASS / FAIL verdict
 *
 * O(n²) scan is fine: K ≤ 1000 → ≤ 500 000 comparisons, < 1 ms.
 */
static void verifySeedDistance(const SeedPoint *seeds, int count,
                               int M, int N, int K)
{
    double d_theory = sqrt((double)(M * N) / (double)K);

    double min_sq = 1e18;
    int    pi = 0, pj = 1;          /* indices of the closest pair */
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            double dx = seeds[i].x - seeds[j].x;
            double dy = seeds[i].y - seeds[j].y;
            double sq = dx * dx + dy * dy;
            if (sq < min_sq) { min_sq = sq; pi = i; pj = j; }
        }
    }
    double actual_min = sqrt(min_sq);

    printf("  ── 最小距离验证 ──────────────────────────────\n");
    printf("  理论下界 d_min = √(M×N/K)\n");
    printf("           = √(%d×%d/%d) = %.4f px\n",
           M, N, K, d_theory);
    printf("  实测最小距离（所有种子对中最小值）= %.4f px\n", actual_min);
    printf("  最近对：种子 #%d (%d,%d) ↔ 种子 #%d (%d,%d)\n",
           seeds[pi].id, seeds[pi].x, seeds[pi].y,
           seeds[pj].id, seeds[pj].x, seeds[pj].y);
    if (actual_min > d_theory - 1e-6)
        printf("  [PASS] 实测 %.4f px > 理论 %.4f px  ✓\n",
               actual_min, d_theory);
    else
        printf("  [FAIL] 实测 %.4f px <= 理论 %.4f px  ✗\n",
               actual_min, d_theory);
    printf("  ─────────────────────────────────────────────\n");
}

/* ── Banner / separator helpers ────────────────────────────────────── */

static void printSep(void) {
    printf("============================================================\n");
}

static void printHeader(const char *title) {
    printSep();
    printf("  %s\n", title);
    printSep();
}

/* ── Input helpers (retry loops – never crash on bad input) ─────────── */

/*
 * readImagePath – prompt for an image path and retry until OpenCV can
 * successfully open it.  Returns the loaded image in `out`.
 */
static void readImagePath(char *pathBuf, int bufSize, cv::Mat &out) {
    while (1) {
        printf("  请输入图片路径：");
        fflush(stdout);

        if (!fgets(pathBuf, bufSize, stdin)) {
            pathBuf[0] = '\0';
            continue;
        }
        /* Strip trailing newline */
        size_t len = strlen(pathBuf);
        if (len > 0 && pathBuf[len - 1] == '\n') pathBuf[len - 1] = '\0';

        if (pathBuf[0] == '\0') {
            printf("  [提示] 路径不能为空，请重新输入。\n");
            continue;
        }

        out = cv::imread(pathBuf, cv::IMREAD_COLOR);
        if (out.empty()) {
            printf("  [提示] 无法读取图片 \"%s\"，请检查路径或文件格式后重新输入。\n",
                   pathBuf);
            continue;
        }
        break; /* success */
    }
}

/*
 * readK – prompt for the seed count K and retry until the input is a
 * positive integer not exceeding the pixel count M*N.
 */
static int readK(int maxPixels) {
    int K = 0;
    while (1) {
        printf("  请输入 K（种子点数量）：");
        fflush(stdout);

        if (scanf("%d", &K) != 1) {
            /* Flush the invalid characters from stdin */
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            printf("  [提示] 输入无效，请输入一个正整数。\n");
            continue;
        }
        getchar(); /* consume trailing newline */

        if (K <= 0) {
            printf("  [提示] K 必须为正整数（输入了 %d），请重新输入。\n", K);
            continue;
        }
        if (K > maxPixels) {
            printf("  [提示] K=%d 超过图像像素总数 %d，请重新输入。\n",
                   K, maxPixels);
            continue;
        }
        break; /* valid */
    }
    return K;
}

/*
 * askSaveImages – ask the user whether to save the result images.
 * Returns 1 for yes, 0 for no.
 */
static int askSaveImages(void) {
    char ans[8];
    while (1) {
        printf("\n  是否保存结果图片？[y/n]：");
        fflush(stdout);
        if (!fgets(ans, sizeof(ans), stdin)) return 0;
        if (ans[0] == 'y' || ans[0] == 'Y') return 1;
        if (ans[0] == 'n' || ans[0] == 'N') return 0;
        printf("  [提示] 请输入 y 或 n。\n");
    }
}

/* ── main ───────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Force line-buffered stdout so the web GUI receives output in real time
       even when running as a subprocess (pipes use full buffering by default). */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* --headless: skip all imshow windows, interactive pauses, and the
       save-images prompt.  Images are always saved automatically.
       Used by the Flask web GUI to avoid blocking on waitKey(). */
    int headless = 0;
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--headless") == 0) { headless = 1; break; }

    printHeader("图像分割课程设计  –  均匀采样 + 四色着色");

    /* ── Step 1: Load image ──────────────────────────────────────────── */
    char img_path[1024] = {0};
    cv::Mat src;
    readImagePath(img_path, sizeof(img_path), src);

    int M = src.rows;   /* image height */
    int N = src.cols;   /* image width  */
    printf("  图片加载成功：%s  （%d × %d）\n", img_path, N, M);

    /* ── Step 2: Read K ─────────────────────────────────────────────── */
    int K = readK(M * N);

    /* ================================================================ */
    printHeader("任务一：种子生成 + Watershed 过分割");

    /* ── Step 3a: Seed generation ────────────────────────────────────── */
    SeedPoint *seeds   = NULL;
    double     t0      = getTimeMs();
    int        actualK = generateSeeds(M, N, K, &seeds);
    printTimeCost("种子生成耗时", t0);
    printf("  请求 K = %d，实际生成种子点：%d\n", K, actualK);
    verifySeedDistance(seeds, actualK, M, N, K);

    /* ── Step 3b: Watershed ─────────────────────────────────────────── */
    cv::Mat markers;
    t0 = getTimeMs();
    runWatershed(src, seeds, actualK, markers);
    printTimeCost("Watershed 耗时", t0);

    /* ── Step 4: Display Task 1 results ─────────────────────────────── */

    /* Window ①: seed annotation (annotate a copy of src in-place) */
    cv::Mat seedVis;
    src.copyTo(seedVis);
    drawSeeds(seedVis, seeds, actualK);
    if (!headless) showImage("① 种子点标注图", seedVis);

    /* Window ②: watershed semi-transparent blend */
    cv::Mat wsVis = visualizeWatershed(src, markers, actualK);
    if (!headless) showImage("② Watershed 半透明着色结果", wsVis);

    /* Save task-1 images immediately in headless mode */
    if (headless) {
        cv::imwrite("result_seeds.png",    seedVis);
        cv::imwrite("result_watershed.png", wsVis);
        printf("  [headless] 已保存 result_seeds.png / result_watershed.png\n");
    }

    /* ── Step 5: Pause before Task 2 ────────────────────────────────── */
    printSep();
    printf("  任务一完成。%s\n", headless ? "（headless 模式，继续任务二）" : "按 Enter 键继续任务二...");
    printSep();
    if (!headless) getchar();

    /* ================================================================ */
    printHeader("任务二：邻接表构建 + 四色着色");

    /* Build region adjacency list */
    t0 = getTimeMs();
    AdjList *adj = buildAdjList(markers, actualK);
    printTimeCost("邻接表构建耗时", t0);

    /* Allocate colour output array (1-indexed: index 0 unused) */
    int *colorOut = (int *)safeMalloc((size_t)(actualK + 1) * sizeof(int));

    /* ── Step 6: Four-colour colouring ──────────────────────────────── */
    t0 = getTimeMs();
    int result = fourColorWatershed(adj, colorOut);
    printTimeCost("四色着色耗时", t0);

    double failRate = coloringFailureRate();
    printf("  回溯失败率：%.2f%%\n", failRate * 100.0);

    cv::Mat colorVis; /* declared here so it stays in scope for saving */

    /* ── Step 7: Report result ──────────────────────────────────────── */
    if (result == COLOR_SUCCESS) {
        printf("  [成功] 四色着色完成，所有区域着色无冲突。\n");

        renderFourColor(markers, actualK, colorOut, colorVis);
        if (!headless) showImage("③ 四色着色结果", colorVis);
    } else {
        printf("  [失败] 四色着色未能完成。\n");
        printf("  失败原因：存在邻居占满 4 色且 Kempe 链修复失败的区域，\n"
               "            图可能含非平面子图（adjacency 检测异常）。\n");
    }

    /* ── Step 8: Save result images ─────────────────────────────────── */
    if (headless) {
        /* Headless: always save all available images automatically */
        if (result == COLOR_SUCCESS && !colorVis.empty())
            cv::imwrite("result_coloring.png", colorVis);
        printf("  [headless] 图片已自动保存。\n");
    } else if (result == COLOR_SUCCESS) {
        if (askSaveImages()) {
            cv::imwrite("result_seeds.png",    seedVis);
            cv::imwrite("result_watershed.png", wsVis);
            cv::imwrite("result_coloring.png",  colorVis);
            printf("  图片已保存：\n");
            printf("    result_seeds.png\n");
            printf("    result_watershed.png\n");
            printf("    result_coloring.png\n");
        } else {
            printf("  结果图片未保存。\n");
        }
    }

    /* ── Step 9: Release all dynamic memory ─────────────────────────── */
    free(colorOut);
    freeAdjList(adj);
    freeSeeds(seeds);

    printSep();
    printf("  程序正常退出。\n");
    printSep();

    return 0;
}
