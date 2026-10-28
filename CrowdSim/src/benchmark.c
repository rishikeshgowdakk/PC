#include "benchmark.h"

static int compare_doubles(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static void compute_stats(double *times, int count, TimingStats *stats)
{
    if (count <= 0) {
        memset(stats, 0, sizeof(TimingStats));
        return;
    }

    qsort(times, (size_t)count, sizeof(double), compare_doubles);

    stats->min_ms = times[0];
    stats->max_ms = times[count - 1];
    stats->median_ms = (count % 2 == 1) ? times[count / 2] : 0.5 * (times[count / 2 - 1] + times[count / 2]);

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += times[i];
    }
    stats->mean_ms = sum / count;

    if (count > 1) {
        double var_sum = 0.0;
        for (int i = 0; i < count; i++) {
            double diff = times[i] - stats->mean_ms;
            var_sum += diff * diff;
        }
        stats->stddev_ms = sqrt(var_sum / (count - 1));
    } else {
        stats->stddev_ms = 0.0;
    }
}

bool benchmark_run_trials(int trials,
                          int agents,
                          int steps,
                          double dt,
                          int threads,
                          ExecMode mode,
                          NeighborMode neighbor_mode,
                          SchedPolicy sched,
                          ScenarioType scenario,
                          unsigned int seed,
                          BenchmarkResult *result)
{
    if (trials < 1) trials = 1;

    double *total_times = (double *)malloc((size_t)trials * sizeof(double));
    double *grid_times = (double *)malloc((size_t)trials * sizeof(double));

    if (!total_times || !grid_times) {
        free(total_times);
        free(grid_times);
        return false;
    }

    result->agents = agents;
    result->steps = steps;
    result->threads = threads;
    result->mode = mode;
    result->neighbor_mode = neighbor_mode;
    result->schedule = sched;
    result->scenario = scenario;
    result->candidate_checks = 0;
    result->actual_neighbors = 0;

    for (int t = 0; t < trials; t++) {
        Simulation sim;
        if (!simulation_init(&sim, agents, steps, dt, threads, mode, neighbor_mode, sched, scenario, seed + (unsigned int)t)) {
            free(total_times);
            free(grid_times);
            return false;
        }

        simulation_run(&sim);

        total_times[t] = sim.total_time_ms;
        grid_times[t] = sim.grid_build_time_ms;
        result->candidate_checks = sim.candidate_checks;
        result->actual_neighbors = sim.actual_neighbors;

        simulation_free(&sim);
    }

    compute_stats(total_times, trials, &result->total_time);
    compute_stats(grid_times, trials, &result->grid_build_time);

    result->speedup = 1.0;
    result->efficiency = 100.0;

    free(total_times);
    free(grid_times);
    return true;
}

void benchmark_run_comprehensive_suite(int agents, int steps, int max_threads, FILE *csv_out)
{
    if (csv_out) {
        fprintf(csv_out, "Scenario,Mode,Algorithm,Schedule,Agents,Steps,Threads,Time_Mean_ms,Time_Median_ms,Time_StdDev_ms,Candidate_Checks,Actual_Neighbors,Speedup,Efficiency\n");
    }

    printf("\n=========================================================================================\n");
    printf("                       CrowdFlow Performance Benchmark Suite\n");
    printf("=========================================================================================\n");
    printf("%-10s %-8s %-6s %-8s %-7s %-10s %-10s %-9s %-10s\n",
           "Algorithm", "Mode", "Threads", "Agents", "Steps", "Mean (ms)", "Med (ms)", "Speedup", "Efficiency");
    printf("-----------------------------------------------------------------------------------------\n");

    /* 1. Baseline Sequential with Grid */
    BenchmarkResult seq_base;
    benchmark_run_trials(3, agents, steps, DEFAULT_DT, 1, MODE_SEQUENTIAL, NEIGHBORS_GRID, SCHED_STATIC, SCENARIO_PILLARS, 42, &seq_base);
    printf("%-10s %-8s %-7d %-8d %-7d %-10.2f %-10.2f %-9.2fx %-9.1f%%\n",
           "Grid", "Seq", 1, agents, steps, seq_base.total_time.mean_ms, seq_base.total_time.median_ms, 1.0, 100.0);
    if (csv_out) {
        fprintf(csv_out, "Pillars,Seq,Grid,Static,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,1.0,100.0\n",
                agents, steps, 1, seq_base.total_time.mean_ms, seq_base.total_time.median_ms, seq_base.total_time.stddev_ms,
                (unsigned long)seq_base.candidate_checks, (unsigned long)seq_base.actual_neighbors);
    }

    /* 2. Parallel Scaling with Grid (Strong Scaling) */
    int thread_counts[] = {1, 2, 4, 8, 12, 16};
    for (size_t i = 0; i < sizeof(thread_counts)/sizeof(thread_counts[0]); i++) {
        int t = thread_counts[i];
        if (t > max_threads) break;

        BenchmarkResult par_res;
        benchmark_run_trials(3, agents, steps, DEFAULT_DT, t, MODE_PARALLEL, NEIGHBORS_GRID, SCHED_STATIC, SCENARIO_PILLARS, 42, &par_res);

        double speedup = (par_res.total_time.mean_ms > 1e-6) ? (seq_base.total_time.mean_ms / par_res.total_time.mean_ms) : 0.0;
        double eff = (speedup / (double)t) * 100.0;

        printf("%-10s %-8s %-7d %-8d %-7d %-10.2f %-10.2f %-9.2fx %-9.1f%%\n",
               "Grid", "Par", t, agents, steps, par_res.total_time.mean_ms, par_res.total_time.median_ms, speedup, eff);

        if (csv_out) {
            fprintf(csv_out, "Pillars,Par,Grid,Static,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,%.3f,%.1f\n",
                    agents, steps, t, par_res.total_time.mean_ms, par_res.total_time.median_ms, par_res.total_time.stddev_ms,
                    (unsigned long)par_res.candidate_checks, (unsigned long)par_res.actual_neighbors, speedup, eff);
        }
    }

    /* 3. Scheduling Comparison on Bottleneck scenario */
    printf("-----------------------------------------------------------------------------------------\n");
    printf(" Scheduling Policies Comparison (Scenario: Bottleneck, Threads: 4)\n");
    printf("-----------------------------------------------------------------------------------------\n");

    SchedPolicy policies[] = {SCHED_STATIC, SCHED_DYNAMIC, SCHED_GUIDED};
    const char *policy_names[] = {"Static", "Dynamic", "Guided"};

    for (int p = 0; p < 3; p++) {
        BenchmarkResult sched_res;
        benchmark_run_trials(3, agents, steps, DEFAULT_DT, 4, MODE_PARALLEL, NEIGHBORS_GRID, policies[p], SCENARIO_BOTTLENECK, 42, &sched_res);
        double speedup = (sched_res.total_time.mean_ms > 1e-6) ? (seq_base.total_time.mean_ms / sched_res.total_time.mean_ms) : 0.0;
        double eff = (speedup / 4.0) * 100.0;

        printf("%-10s %-8s %-7d %-8d %-7d %-10.2f %-10.2f %-9.2fx %-9.1f%% (Sched: %s)\n",
               "Grid", "Par", 4, agents, steps, sched_res.total_time.mean_ms, sched_res.total_time.median_ms, speedup, eff, policy_names[p]);

        if (csv_out) {
            fprintf(csv_out, "Bottleneck,Par,Grid,%s,%d,%d,%d,%.3f,%.3f,%.3f,%lu,%lu,%.3f,%.1f\n",
                    policy_names[p], agents, steps, 4, sched_res.total_time.mean_ms, sched_res.total_time.median_ms, sched_res.total_time.stddev_ms,
                    (unsigned long)sched_res.candidate_checks, (unsigned long)sched_res.actual_neighbors, speedup, eff);
        }
    }

    printf("=========================================================================================\n\n");
}
