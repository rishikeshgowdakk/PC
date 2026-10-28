#include "common.h"
#include "agent.h"
#include "obstacle.h"
#include "spatial_grid.h"
#include "simulation.h"
#include "benchmark.h"

static void print_usage(const char *prog)
{
    printf("CrowdFlow — A Parallel Crowd Dynamics Simulator & OpenMP Performance Analysis Platform\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Core Options:\n");
    printf("  --agents N            Number of agents (default: 10000, max: %d)\n", MAX_AGENTS);
    printf("  --steps N             Number of simulation steps (default: 100, max: %d)\n", MAX_STEPS);
    printf("  --threads N           Number of OpenMP threads (default: 4, max: %d)\n", MAX_THREADS);
    printf("  --mode MODE           Execution mode: 'parallel' or 'sequential' (default: parallel)\n");
    printf("  --neighbors ALGO      Neighbor detection: 'grid' or 'brute' (default: grid)\n");
    printf("  --schedule SCHED      OpenMP scheduling: 'static', 'dynamic', 'guided' (default: static)\n");
    printf("  --scenario SCEN       Scenario layout: 'empty', 'pillars', 'bottleneck' (default: pillars)\n");
    printf("  --seed N              Random seed for deterministic init (default: 42)\n");
    printf("  --dt F                Time step delta in seconds (default: 0.05)\n\n");
    printf("Modes & Output Options:\n");
    printf("  --compare             Run both Sequential & Parallel to verify exact state match\n");
    printf("  --benchmark           Execute comprehensive multi-thread scaling benchmark suite\n");
    printf("  --trials N            Number of repetitions for benchmark averaging (default: 1)\n");
    printf("  --csv                 Output machine-readable CSV performance data\n");
    printf("  --json                Output structured JSON for the web dashboard\n");
    printf("  --help                Display this documentation synopsis\n");
}

static long parse_pos_int(const char *s, const char *name)
{
    if (!s || !*s) {
        fprintf(stderr, "Error: %s requires a positive integer value.\n", name);
        exit(1);
    }
    char *end;
    long val = strtol(s, &end, 10);
    if (*end != '\0' || val <= 0) {
        fprintf(stderr, "Error: %s must be a positive integer, got '%s'.\n", name, s);
        exit(1);
    }
    return val;
}

int main(int argc, char *argv[])
{
    int agents = 10000;
    int steps = 100;
    int threads = 4;
    double dt = DEFAULT_DT;
    ExecMode mode = MODE_PARALLEL;
    NeighborMode neighbor_mode = NEIGHBORS_GRID;
    SchedPolicy sched = SCHED_STATIC;
    ScenarioType scenario = SCENARIO_PILLARS;
    unsigned int seed = 42;
    int trials = 1;

    bool run_compare = false;
    bool run_bench_suite = false;
    bool output_csv = false;
    bool output_json = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--agents") == 0 || strcmp(argv[i], "-a") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --agents requires a value.\n"); return 1; }
            agents = (int)parse_pos_int(argv[i], "--agents");
            if (agents > MAX_AGENTS) { fprintf(stderr, "Error: agents cannot exceed %d\n", MAX_AGENTS); return 1; }
        } else if (strcmp(argv[i], "--steps") == 0 || strcmp(argv[i], "-s") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --steps requires a value.\n"); return 1; }
            steps = (int)parse_pos_int(argv[i], "--steps");
            if (steps > MAX_STEPS) { fprintf(stderr, "Error: steps cannot exceed %d\n", MAX_STEPS); return 1; }
        } else if (strcmp(argv[i], "--threads") == 0 || strcmp(argv[i], "-t") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --threads requires a value.\n"); return 1; }
            threads = (int)parse_pos_int(argv[i], "--threads");
            if (threads > MAX_THREADS) { fprintf(stderr, "Error: threads cannot exceed %d\n", MAX_THREADS); return 1; }
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --mode requires a value.\n"); return 1; }
            if (strcmp(argv[i], "parallel") == 0) mode = MODE_PARALLEL;
            else if (strcmp(argv[i], "sequential") == 0) mode = MODE_SEQUENTIAL;
            else { fprintf(stderr, "Error: unknown mode '%s' (use 'parallel' or 'sequential')\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--neighbors") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --neighbors requires a value.\n"); return 1; }
            if (strcmp(argv[i], "grid") == 0) neighbor_mode = NEIGHBORS_GRID;
            else if (strcmp(argv[i], "brute") == 0) neighbor_mode = NEIGHBORS_BRUTE_FORCE;
            else { fprintf(stderr, "Error: unknown neighbor algorithm '%s' (use 'grid' or 'brute')\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--schedule") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --schedule requires a value.\n"); return 1; }
            if (strcmp(argv[i], "static") == 0) sched = SCHED_STATIC;
            else if (strcmp(argv[i], "dynamic") == 0) sched = SCHED_DYNAMIC;
            else if (strcmp(argv[i], "guided") == 0) sched = SCHED_GUIDED;
            else { fprintf(stderr, "Error: unknown schedule '%s' (use 'static', 'dynamic', 'guided')\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--scenario") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --scenario requires a value.\n"); return 1; }
            if (strcmp(argv[i], "empty") == 0) scenario = SCENARIO_EMPTY;
            else if (strcmp(argv[i], "pillars") == 0) scenario = SCENARIO_PILLARS;
            else if (strcmp(argv[i], "bottleneck") == 0) scenario = SCENARIO_BOTTLENECK;
            else { fprintf(stderr, "Error: unknown scenario '%s' (use 'empty', 'pillars', 'bottleneck')\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--seed") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --seed requires a value.\n"); return 1; }
            seed = (unsigned int)parse_pos_int(argv[i], "--seed");
        } else if (strcmp(argv[i], "--dt") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --dt requires a value.\n"); return 1; }
            dt = atof(argv[i]);
            if (dt <= 0.0 || dt > 1.0) { fprintf(stderr, "Error: dt must be between 0.001 and 1.0\n"); return 1; }
        } else if (strcmp(argv[i], "--trials") == 0) {
            if (++i >= argc) { fprintf(stderr, "Error: --trials requires a value.\n"); return 1; }
            trials = (int)parse_pos_int(argv[i], "--trials");
        } else if (strcmp(argv[i], "--compare") == 0) {
            run_compare = true;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            run_bench_suite = true;
        } else if (strcmp(argv[i], "--csv") == 0) {
            output_csv = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            output_json = true;
        } else {
            fprintf(stderr, "Error: unknown option '%s'. Run with --help for options.\n", argv[i]);
            return 1;
        }
    }

    if (run_bench_suite) {
        benchmark_run_comprehensive_suite(agents, steps, threads, output_csv ? stdout : NULL);
        return 0;
    }

    if (run_compare) {
        /* Run both Sequential and Parallel engines with identical parameters and verify match */
        Simulation sim_seq, sim_par;
        if (!simulation_init(&sim_seq, agents, steps, dt, 1, MODE_SEQUENTIAL, neighbor_mode, sched, scenario, seed)) {
            fprintf(stderr, "Error initializing sequential simulation.\n");
            return 1;
        }
        if (!simulation_init(&sim_par, agents, steps, dt, threads, MODE_PARALLEL, neighbor_mode, sched, scenario, seed)) {
            fprintf(stderr, "Error initializing parallel simulation.\n");
            simulation_free(&sim_seq);
            return 1;
        }

        simulation_run(&sim_seq);
        simulation_run(&sim_par);

        int mismatch_id = -1;
        bool match = simulation_validate_match(&sim_seq, &sim_par, TOLERANCE_DEFAULT, &mismatch_id);

        char err_buf[256] = {0};
        bool bounds_seq = simulation_validate_invariants(&sim_seq, err_buf, sizeof(err_buf));
        bool bounds_par = simulation_validate_invariants(&sim_par, err_buf, sizeof(err_buf));

        double speedup = (sim_par.total_time_ms > 1e-6) ? (sim_seq.total_time_ms / sim_par.total_time_ms) : 0.0;
        double eff = (speedup / (double)threads) * 100.0;

        if (output_json) {
            int grid_rows = 30, grid_cols = 80;
            int *density = (int *)malloc((size_t)(grid_rows * grid_cols) * sizeof(int));
            simulation_get_density(&sim_par, grid_rows, grid_cols, density);

            printf("{");
            printf("\"agents\":%d,", agents);
            printf("\"steps\":%d,", steps);
            printf("\"threads\":%d,", threads);
            printf("\"mode\":\"compare\",");
            printf("\"neighbors\":\"%s\",", (neighbor_mode == NEIGHBORS_GRID) ? "grid" : "brute");
            printf("\"scenario\":\"%s\",", (scenario == SCENARIO_PILLARS) ? "pillars" : ((scenario == SCENARIO_BOTTLENECK) ? "bottleneck" : "empty"));
            printf("\"seq_time_ms\":%.3f,", sim_seq.total_time_ms);
            printf("\"par_time_ms\":%.3f,", sim_par.total_time_ms);
            printf("\"speedup\":%.4f,", speedup);
            printf("\"efficiency\":%.2f,", eff);
            printf("\"candidate_checks\":%lu,", (unsigned long)sim_par.candidate_checks);
            printf("\"actual_neighbors\":%lu,", (unsigned long)sim_par.actual_neighbors);
            printf("\"match\":%s,", match ? "true" : "false");
            printf("\"bounds\":%s,", (bounds_seq && bounds_par) ? "true" : "false");
            printf("\"grid_cols\":%d,\"grid_rows\":%d,", grid_cols, grid_rows);
            printf("\"density\":[");
            for (int r = 0; r < grid_rows; r++) {
                if (r > 0) printf(",");
                printf("[");
                for (int c = 0; c < grid_cols; c++) {
                    if (c > 0) printf(",");
                    printf("%d", density[r * grid_cols + c]);
                }
                printf("]");
            }
            printf("],");

            /* Sample of agent coordinates for detailed canvas representation */
            int sample_count = (agents < 300) ? agents : 300;
            printf("\"sample_agents\":[");
            for (int i = 0; i < sample_count; i++) {
                if (i > 0) printf(",");
                const Agent *a = &sim_par.current_agents[i];
                printf("{\"id\":%d,\"x\":%.2f,\"y\":%.2f,\"vx\":%.2f,\"vy\":%.2f,\"gx\":%.2f,\"gy\":%.2f}",
                       a->id, a->x, a->y, a->vx, a->vy, a->gx, a->gy);
            }
            printf("],");

            /* Obstacles list */
            printf("\"obstacles\":[");
            for (int i = 0; i < sim_par.obstacles.count; i++) {
                if (i > 0) printf(",");
                const Obstacle *obs = &sim_par.obstacles.obstacles[i];
                if (obs->type == OBSTACLE_CIRCLE) {
                    printf("{\"type\":\"circle\",\"cx\":%.2f,\"cy\":%.2f,\"radius\":%.2f}", obs->cx, obs->cy, obs->radius);
                } else {
                    printf("{\"type\":\"aabb\",\"min_x\":%.2f,\"min_y\":%.2f,\"max_x\":%.2f,\"max_y\":%.2f}",
                           obs->min_x, obs->min_y, obs->max_x, obs->max_y);
                }
            }
            printf("]");
            printf("}\n");

            free(density);
        } else {
            printf("\n========================================================\n");
            printf("   CrowdFlow — Sequential vs Parallel Equivalence Run\n");
            printf("========================================================\n");
            printf("Agents:                %d\n", agents);
            printf("Steps:                 %d (dt = %.3f s)\n", steps, dt);
            printf("Threads:               %d\n", threads);
            printf("Algorithm:             %s\n", (neighbor_mode == NEIGHBORS_GRID) ? "Uniform Spatial Grid" : "Brute Force");
            printf("Scenario:              %s\n", (scenario == SCENARIO_PILLARS) ? "Pillars" : ((scenario == SCENARIO_BOTTLENECK) ? "Bottleneck Door" : "Empty Arena"));
            printf("Sequential time:       %.3f ms\n", sim_seq.total_time_ms);
            printf("Parallel time:         %.3f ms\n", sim_par.total_time_ms);
            printf("Speedup:               %.2fx\n", speedup);
            printf("Parallel efficiency:   %.1f%%\n", eff);
            printf("Candidate checks:      %lu\n", (unsigned long)sim_par.candidate_checks);
            printf("Actual neighbors:      %lu\n", (unsigned long)sim_par.actual_neighbors);
            printf("State match:           %s\n", match ? "MATCH (verified <= 1e-6)" : "MISMATCH");
            printf("Invariant validation:  %s\n", (bounds_seq && bounds_par) ? "PASS" : "FAIL");
            if (!match) {
                printf("  -> Mismatch at agent ID %d\n", mismatch_id);
            }
            if (!bounds_par) {
                printf("  -> Invariant error: %s\n", err_buf);
            }
            printf("========================================================\n\n");
        }

        simulation_free(&sim_seq);
        simulation_free(&sim_par);
        return (match && bounds_seq && bounds_par) ? 0 : 1;
    }

    /* Single Run Mode (or with trials) */
    BenchmarkResult bench;
    if (!benchmark_run_trials(trials, agents, steps, dt, threads, mode, neighbor_mode, sched, scenario, seed, &bench)) {
        fprintf(stderr, "Error executing simulation.\n");
        return 1;
    }

    if (output_csv) {
        printf("Mode,Algorithm,Schedule,Scenario,Agents,Steps,Threads,Time_Mean_ms,Time_Median_ms,Time_StdDev_ms,Candidate_Checks,Actual_Neighbors\n");
        printf("%s,%s,%s,%s,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu\n",
               (mode == MODE_PARALLEL) ? "Parallel" : "Sequential",
               (neighbor_mode == NEIGHBORS_GRID) ? "Grid" : "BruteForce",
               (sched == SCHED_STATIC) ? "Static" : ((sched == SCHED_DYNAMIC) ? "Dynamic" : "Guided"),
               (scenario == SCENARIO_PILLARS) ? "Pillars" : ((scenario == SCENARIO_BOTTLENECK) ? "Bottleneck" : "Empty"),
               agents, steps, threads,
               bench.total_time.mean_ms, bench.total_time.median_ms, bench.total_time.stddev_ms,
               (unsigned long)bench.candidate_checks, (unsigned long)bench.actual_neighbors);
    } else if (output_json) {
        printf("{\"agents\":%d,\"steps\":%d,\"threads\":%d,\"mode\":\"%s\",\"neighbors\":\"%s\",\"time_ms\":%.3f,\"grid_build_ms\":%.3f,\"candidate_checks\":%lu,\"actual_neighbors\":%lu}\n",
               agents, steps, threads,
               (mode == MODE_PARALLEL) ? "parallel" : "sequential",
               (neighbor_mode == NEIGHBORS_GRID) ? "grid" : "brute",
               bench.total_time.mean_ms, bench.grid_build_time.mean_ms,
               (unsigned long)bench.candidate_checks, (unsigned long)bench.actual_neighbors);
    } else {
        printf("\n========================================================\n");
        printf("   CrowdFlow — Simulation Execution Summary\n");
        printf("========================================================\n");
        printf("Agents:                %d\n", agents);
        printf("Steps:                 %d (dt = %.3f s)\n", steps, dt);
        printf("Threads:               %d\n", (mode == MODE_PARALLEL) ? threads : 1);
        printf("Execution mode:        %s\n", (mode == MODE_PARALLEL) ? "OpenMP Parallel" : "Sequential");
        printf("Algorithm:             %s\n", (neighbor_mode == NEIGHBORS_GRID) ? "Uniform Spatial Grid" : "Brute Force");
        printf("Schedule:              %s\n", (sched == SCHED_STATIC) ? "Static" : ((sched == SCHED_DYNAMIC) ? "Dynamic" : "Guided"));
        printf("Scenario:              %s\n", (scenario == SCENARIO_PILLARS) ? "Pillars" : ((scenario == SCENARIO_BOTTLENECK) ? "Bottleneck Door" : "Empty Arena"));
        printf("Mean execution time:   %.3f ms (stddev: %.3f ms)\n", bench.total_time.mean_ms, bench.total_time.stddev_ms);
        printf("Median execution time: %.3f ms\n", bench.total_time.median_ms);
        if (neighbor_mode == NEIGHBORS_GRID) {
            printf("Grid build time:       %.3f ms\n", bench.grid_build_time.mean_ms);
        }
        printf("Candidate checks:      %lu\n", (unsigned long)bench.candidate_checks);
        printf("Actual neighbors:      %lu\n", (unsigned long)bench.actual_neighbors);
        printf("========================================================\n\n");
    }

    return 0;
}
