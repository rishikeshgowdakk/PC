#ifndef CROWDFLOW_BENCHMARK_H
#define CROWDFLOW_BENCHMARK_H

#include "common.h"
#include "simulation.h"

typedef struct {
    double min_ms;
    double max_ms;
    double mean_ms;
    double median_ms;
    double stddev_ms;
} TimingStats;

typedef struct {
    int agents;
    int steps;
    int threads;
    ExecMode mode;
    NeighborMode neighbor_mode;
    SchedPolicy schedule;
    ScenarioType scenario;

    TimingStats total_time;
    TimingStats grid_build_time;
    uint64_t candidate_checks;
    uint64_t actual_neighbors;

    double speedup;       /* Speedup relative to sequential baseline */
    double efficiency;    /* Parallel efficiency percentage */
} BenchmarkResult;

/* Run multiple trials of a simulation and compute statistical distributions */
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
                          BenchmarkResult *result);

/* Run a full comparative scaling matrix (Strong Scaling & Algorithmic Scaling) */
void benchmark_run_comprehensive_suite(int agents, int steps, int max_threads, FILE *csv_out);

#endif /* CROWDFLOW_BENCHMARK_H */
