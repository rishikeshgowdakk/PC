#ifndef CROWDFLOW_SIMULATION_H
#define CROWDFLOW_SIMULATION_H

#include "common.h"
#include "agent.h"
#include "obstacle.h"
#include "spatial_grid.h"

typedef struct {
    Agent *current_agents;
    Agent *next_agents;
    int num_agents;
    int steps;
    double dt;
    int num_threads;
    ExecMode mode;
    NeighborMode neighbor_mode;
    SchedPolicy schedule;
    ScenarioType scenario;
    unsigned int seed;
    double interact_radius;

    ObstacleSet obstacles;
    SpatialGrid grid;

    /* Metrics */
    double total_time_ms;
    double grid_build_time_ms;
    double force_calc_time_ms;
    uint64_t candidate_checks;
    uint64_t actual_neighbors;
} Simulation;

/* Initialize a simulation context and allocate agent buffers */
bool simulation_init(Simulation *sim,
                     int num_agents,
                     int steps,
                     double dt,
                     int threads,
                     ExecMode mode,
                     NeighborMode neighbor_mode,
                     SchedPolicy sched,
                     ScenarioType scenario,
                     unsigned int seed);

/* Free all resources associated with a simulation */
void simulation_free(Simulation *sim);

/* Execute the simulation over the configured step count */
void simulation_run(Simulation *sim);

/* Validate that two simulation final states match within floating-point tolerance */
bool simulation_validate_match(const Simulation *sim1, const Simulation *sim2, double tolerance, int *mismatch_id);

/* Validate simulation physical invariants: finite coordinates, world boundary containment, no obstacle penetration */
bool simulation_validate_invariants(const Simulation *sim, char *err_buf, size_t err_size);

/* Compute spatial density matrix for ASCII or Canvas visualization */
void simulation_get_density(const Simulation *sim, int rows, int cols, int *density_grid);

#endif /* CROWDFLOW_SIMULATION_H */
