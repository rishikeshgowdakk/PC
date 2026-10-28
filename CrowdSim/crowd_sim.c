/*
 * CrowdSim — Sequential and OpenMP-Based Parallel Particle Movement Simulator
 *
 * A simple particle-movement simulation that demonstrates shared-memory
 * parallelism using OpenMP.  Each particle moves inside a bounded 2-D
 * rectangle and bounces off walls.  The program compares sequential and
 * parallel execution, measures performance, and validates correctness.
 *
 * Build:  gcc -std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic \
 *              crowd_sim.c -o crowd_sim -lm
 * Usage:  ./crowd_sim [--particles N] [--steps N] [--threads N] [--json] [--help]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

/* ── environment constants ───────────────────────────────────────────── */
#define ENV_WIDTH   100.0
#define ENV_HEIGHT   40.0

/* ── default simulation parameters ───────────────────────────────────── */
#define DEFAULT_PARTICLES  50000
#define DEFAULT_STEPS        500
#define DEFAULT_THREADS        4

/* ── validation tolerance ────────────────────────────────────────────── */
#define TOLERANCE  1e-9

/* ── ASCII grid dimensions (terminal mode) ───────────────────────────── */
#define GRID_COLS  50
#define GRID_ROWS  15

/* ── density grid for JSON mode (higher resolution for canvas) ───────── */
#define JSON_GRID_COLS  80
#define JSON_GRID_ROWS  30

/* ── hard limits to prevent accidental huge allocations ───────────────── */
#define MAX_PARTICLES  50000000   /* 50 million */
#define MAX_STEPS      10000000
#define MAX_THREADS       1024

/* ── particle structure ──────────────────────────────────────────────── */
typedef struct {
    double x;
    double y;
    double vx;
    double vy;
} Particle;

/* ──────────────────────────────────────────────────────────────────────
 * Deterministic particle initialisation.
 * Uses a simple formula so every run starts from the same state.
 * ────────────────────────────────────────────────────────────────────── */
static void init_particles(Particle *p, int n)
{
    for (int i = 0; i < n; i++) {
        /* spread particles across the environment */
        p[i].x  = fmod((double)(i * 7 + 13), ENV_WIDTH);
        p[i].y  = fmod((double)(i * 3 + 7),  ENV_HEIGHT);

        /* give each particle a unique velocity in [-2, 2] */
        p[i].vx = sin((double)i * 0.1)  * 2.0;
        p[i].vy = cos((double)i * 0.07) * 2.0;
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Reflect a coordinate into [0, limit] and adjust the velocity sign.
 *
 * Works for arbitrary overshoot: the particle "bounces" back and forth
 * inside the interval as many times as needed.
 * ────────────────────────────────────────────────────────────────────── */
static void reflect(double *pos, double *vel, double limit)
{
    /* Normalise into [0, 2*limit] period */
    double p = *pos;
    if (p < 0.0 || p > limit) {
        double period = 2.0 * limit;
        /* bring into [0, period) */
        p = fmod(p, period);
        if (p < 0.0)
            p += period;
        /* fold the second half back */
        if (p > limit) {
            p = period - p;
            *vel = -(*vel);
        }
    }
    *pos = p;
}

/* ──────────────────────────────────────────────────────────────────────
 * Sequential simulation — reference implementation.
 * ────────────────────────────────────────────────────────────────────── */
static void simulate_sequential(Particle *p, int n, int steps)
{
    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < n; i++) {
            p[i].x += p[i].vx;
            p[i].y += p[i].vy;
            reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
            reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Parallel simulation — OpenMP version.
 * Each particle is independent, so the inner loop is trivially parallel.
 * ────────────────────────────────────────────────────────────────────── */
static void simulate_parallel(Particle *p, int n, int steps)
{
    for (int s = 0; s < steps; s++) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < n; i++) {
            p[i].x += p[i].vx;
            p[i].y += p[i].vy;
            reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
            reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Verify that every particle is inside the simulation boundaries.
 * Returns 1 on success, 0 on failure.
 * ────────────────────────────────────────────────────────────────────── */
static int validate_bounds(const Particle *p, int n)
{
    for (int i = 0; i < n; i++) {
        if (p[i].x < -TOLERANCE || p[i].x > ENV_WIDTH  + TOLERANCE ||
            p[i].y < -TOLERANCE || p[i].y > ENV_HEIGHT + TOLERANCE) {
            fprintf(stderr, "Boundary error: particle %d at (%.6f, %.6f)\n",
                    i, p[i].x, p[i].y);
            return 0;
        }
    }
    return 1;
}

/* ──────────────────────────────────────────────────────────────────────
 * Compare two particle arrays element-by-element.
 * Returns 1 if they match within TOLERANCE, 0 otherwise.
 * ────────────────────────────────────────────────────────────────────── */
static int compare_results(const Particle *a, const Particle *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (fabs(a[i].x  - b[i].x)  > TOLERANCE ||
            fabs(a[i].y  - b[i].y)  > TOLERANCE ||
            fabs(a[i].vx - b[i].vx) > TOLERANCE ||
            fabs(a[i].vy - b[i].vy) > TOLERANCE) {
            fprintf(stderr, "Mismatch at particle %d:\n"
                    "  seq  (%.10f, %.10f, %.10f, %.10f)\n"
                    "  par  (%.10f, %.10f, %.10f, %.10f)\n",
                    i,
                    a[i].x,  a[i].y,  a[i].vx,  a[i].vy,
                    b[i].x,  b[i].y,  b[i].vx,  b[i].vy);
            return 0;
        }
    }
    return 1;
}

/* ──────────────────────────────────────────────────────────────────────
 * Print a compact ASCII map of the final particle distribution.
 * Cells with more particles get a denser character.
 * ────────────────────────────────────────────────────────────────────── */
static void print_grid(const Particle *p, int n)
{
    int grid[GRID_ROWS][GRID_COLS];
    memset(grid, 0, sizeof(grid));

    for (int i = 0; i < n; i++) {
        int col = (int)(p[i].x / ENV_WIDTH  * GRID_COLS);
        int row = (int)(p[i].y / ENV_HEIGHT * GRID_ROWS);
        if (col < 0) col = 0;
        if (col >= GRID_COLS) col = GRID_COLS - 1;
        if (row < 0) row = 0;
        if (row >= GRID_ROWS) row = GRID_ROWS - 1;
        grid[row][col]++;
    }

    /* top border */
    printf("    +");
    for (int c = 0; c < GRID_COLS; c++) putchar('-');
    printf("+\n");

    for (int r = 0; r < GRID_ROWS; r++) {
        printf("    |");
        for (int c = 0; c < GRID_COLS; c++) {
            int v = grid[r][c];
            if      (v == 0) putchar(' ');
            else if (v == 1) putchar('.');
            else if (v <  5) putchar('*');
            else if (v < 20) putchar('#');
            else             putchar('@');
        }
        printf("|\n");
    }

    /* bottom border */
    printf("    +");
    for (int c = 0; c < GRID_COLS; c++) putchar('-');
    printf("+\n");

    printf("    Legend: . = 1  * = 2-4  # = 5-19  @ = 20+\n");
}

/* ──────────────────────────────────────────────────────────────────────
 * Build a density grid and print it as a JSON 2-D array.
 * Used by the web backend to render the Canvas visualisation.
 * ────────────────────────────────────────────────────────────────────── */
static void print_density_json(const Particle *p, int n)
{
    int grid[JSON_GRID_ROWS][JSON_GRID_COLS];
    memset(grid, 0, sizeof(grid));

    for (int i = 0; i < n; i++) {
        int col = (int)(p[i].x / ENV_WIDTH  * JSON_GRID_COLS);
        int row = (int)(p[i].y / ENV_HEIGHT * JSON_GRID_ROWS);
        if (col < 0) col = 0;
        if (col >= JSON_GRID_COLS) col = JSON_GRID_COLS - 1;
        if (row < 0) row = 0;
        if (row >= JSON_GRID_ROWS) row = JSON_GRID_ROWS - 1;
        grid[row][col]++;
    }

    printf("[");
    for (int r = 0; r < JSON_GRID_ROWS; r++) {
        if (r > 0) printf(",");
        printf("[");
        for (int c = 0; c < JSON_GRID_COLS; c++) {
            if (c > 0) printf(",");
            printf("%d", grid[r][c]);
        }
        printf("]");
    }
    printf("]");
}

/* ──────────────────────────────────────────────────────────────────────
 * Print usage information.
 * ────────────────────────────────────────────────────────────────────── */
static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n\n"
           "Options:\n"
           "  --particles N   Number of particles (default %d, max %d)\n"
           "  --steps N       Number of simulation steps (default %d, max %d)\n"
           "  --threads N     Number of OpenMP threads (default %d, max %d)\n"
           "  --json          Output results as JSON (for web backend)\n"
           "  --help          Show this help message\n",
           prog,
           DEFAULT_PARTICLES, MAX_PARTICLES,
           DEFAULT_STEPS,     MAX_STEPS,
           DEFAULT_THREADS,   MAX_THREADS);
}

/* ──────────────────────────────────────────────────────────────────────
 * Parse a positive integer from a string.
 * Returns -1 on any error (non-numeric, zero, negative, overflow).
 * ────────────────────────────────────────────────────────────────────── */
static long parse_positive_int(const char *s)
{
    if (s == NULL || *s == '\0')
        return -1;

    char *end;
    long val = strtol(s, &end, 10);

    /* reject trailing garbage, non-positive values, and overflow */
    if (*end != '\0' || val <= 0 || end == s)
        return -1;

    return val;
}

/* ── main ────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    int num_particles = DEFAULT_PARTICLES;
    int num_steps     = DEFAULT_STEPS;
    int num_threads   = DEFAULT_THREADS;
    int json_mode     = 0;

    /* ── parse command-line arguments ────────────────────────────────── */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = 1;
        } else if (strcmp(argv[i], "--particles") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --particles requires a value.\n");
                return 1;
            }
            long v = parse_positive_int(argv[++i]);
            if (v < 0 || v > MAX_PARTICLES) {
                fprintf(stderr,
                        "Error: --particles must be between 1 and %d.\n",
                        MAX_PARTICLES);
                return 1;
            }
            num_particles = (int)v;
        } else if (strcmp(argv[i], "--steps") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --steps requires a value.\n");
                return 1;
            }
            long v = parse_positive_int(argv[++i]);
            if (v < 0 || v > MAX_STEPS) {
                fprintf(stderr,
                        "Error: --steps must be between 1 and %d.\n",
                        MAX_STEPS);
                return 1;
            }
            num_steps = (int)v;
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --threads requires a value.\n");
                return 1;
            }
            long v = parse_positive_int(argv[++i]);
            if (v < 0 || v > MAX_THREADS) {
                fprintf(stderr,
                        "Error: --threads must be between 1 and %d.\n",
                        MAX_THREADS);
                return 1;
            }
            num_threads = (int)v;
        } else {
            fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* ── set OpenMP thread count ────────────────────────────────────── */
    omp_set_num_threads(num_threads);

    /* ── allocate particle arrays ───────────────────────────────────── */
    size_t sz = (size_t)num_particles * sizeof(Particle);
    Particle *init  = malloc(sz);   /* master copy */
    Particle *seq   = malloc(sz);   /* sequential working copy */
    Particle *par   = malloc(sz);   /* parallel working copy */

    if (!init || !seq || !par) {
        fprintf(stderr, "Error: memory allocation failed for %d particles.\n",
                num_particles);
        free(init); free(seq); free(par);
        return 1;
    }

    /* ── deterministic initialisation ───────────────────────────────── */
    init_particles(init, num_particles);

    /* ── prepare working copies ─────────────────────────────────────── */
    memcpy(seq, init, sz);
    memcpy(par, init, sz);

    /* ── sequential run ─────────────────────────────────────────────── */
    double t_seq_start = omp_get_wtime();
    simulate_sequential(seq, num_particles, num_steps);
    double t_seq_end   = omp_get_wtime();
    double t_seq_ms    = (t_seq_end - t_seq_start) * 1000.0;

    /* ── parallel run ───────────────────────────────────────────────── */
    double t_par_start = omp_get_wtime();
    simulate_parallel(par, num_particles, num_steps);
    double t_par_end   = omp_get_wtime();
    double t_par_ms    = (t_par_end - t_par_start) * 1000.0;

    /* ── performance metrics ────────────────────────────────────────── */
    double speedup    = (t_par_ms > 1e-6) ? t_seq_ms / t_par_ms : 0.0;
    double efficiency = speedup / (double)num_threads * 100.0;

    /* ── correctness checks ─────────────────────────────────────────── */
    int match  = compare_results(seq, par, num_particles);
    int bounds = validate_bounds(seq, num_particles) &&
                 validate_bounds(par, num_particles);

    if (json_mode) {
        /* ── JSON output for the web backend ─────────────────────────── */
        printf("{");
        printf("\"particles\":%d,", num_particles);
        printf("\"steps\":%d,", num_steps);
        printf("\"threads\":%d,", num_threads);
        printf("\"seq_time_ms\":%.3f,", t_seq_ms);
        printf("\"par_time_ms\":%.3f,", t_par_ms);
        printf("\"speedup\":%.4f,", speedup);
        printf("\"efficiency\":%.2f,", efficiency);
        printf("\"match\":%s,", match ? "true" : "false");
        printf("\"bounds\":%s,", bounds ? "true" : "false");
        printf("\"env_width\":%.1f,", ENV_WIDTH);
        printf("\"env_height\":%.1f,", ENV_HEIGHT);
        printf("\"grid_cols\":%d,", JSON_GRID_COLS);
        printf("\"grid_rows\":%d,", JSON_GRID_ROWS);
        printf("\"density\":");
        print_density_json(seq, num_particles);
        printf("}\n");
    } else {
        /* ── normal terminal output ──────────────────────────────────── */
        printf("\n==========================================\n");
        printf("   CrowdSim - Parallel Particle Simulator\n");
        printf("==========================================\n\n");
        printf("Particles:             %d\n",   num_particles);
        printf("Simulation steps:      %d\n",   num_steps);
        printf("OpenMP threads:        %d\n\n", num_threads);

        printf("Sequential time:       %.3f ms\n",  t_seq_ms);
        printf("Parallel time:         %.3f ms\n",  t_par_ms);
        printf("Speedup:               %.2fx\n",    speedup);
        printf("Parallel efficiency:   %.1f%%\n\n", efficiency);

        printf("Sequential/parallel results: %s\n", match  ? "MATCH" : "MISMATCH");
        printf("Boundary validation:         %s\n", bounds ? "PASS"  : "FAIL");

        printf("\nFinal particle distribution (sequential):\n");
        print_grid(seq, num_particles);
        printf("\n");
    }

    /* ── clean up ───────────────────────────────────────────────────── */
    free(init);
    free(seq);
    free(par);

    if (!match || !bounds) {
        fprintf(stderr, "VERIFICATION FAILED.\n");
        return 1;
    }

    return 0;
}
