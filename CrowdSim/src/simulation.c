#include "simulation.h"

bool simulation_init(Simulation *sim,
                     int num_agents,
                     int steps,
                     double dt,
                     int threads,
                     ExecMode mode,
                     NeighborMode neighbor_mode,
                     SchedPolicy sched,
                     ScenarioType scenario,
                     unsigned int seed)
{
    sim->num_agents = num_agents;
    sim->steps = steps;
    sim->dt = dt;
    sim->num_threads = (threads < 1) ? 1 : threads;
    sim->mode = mode;
    sim->neighbor_mode = neighbor_mode;
    sim->schedule = sched;
    sim->scenario = scenario;
    sim->seed = seed;
    sim->interact_radius = DEFAULT_INTERACT_RADIUS;

    sim->total_time_ms = 0.0;
    sim->grid_build_time_ms = 0.0;
    sim->force_calc_time_ms = 0.0;
    sim->candidate_checks = 0;
    sim->actual_neighbors = 0;

    size_t sz = (size_t)num_agents * sizeof(Agent);
    sim->current_agents = (Agent *)malloc(sz);
    sim->next_agents = (Agent *)malloc(sz);

    if (!sim->current_agents || !sim->next_agents) {
        free(sim->current_agents);
        free(sim->next_agents);
        sim->current_agents = NULL;
        sim->next_agents = NULL;
        return false;
    }

    obstacle_init_set(&sim->obstacles, scenario);

    /* Cell size >= interaction radius ensures 3x3 cell neighborhood covers all potential neighbors */
    if (!spatial_grid_init(&sim->grid, ENV_WIDTH, ENV_HEIGHT, sim->interact_radius, num_agents)) {
        free(sim->current_agents);
        free(sim->next_agents);
        sim->current_agents = NULL;
        sim->next_agents = NULL;
        return false;
    }

    agent_init_population(sim->current_agents, num_agents, scenario, seed);
    memcpy(sim->next_agents, sim->current_agents, sz);
    return true;
}

void simulation_free(Simulation *sim)
{
    if (sim->current_agents) {
        free(sim->current_agents);
        sim->current_agents = NULL;
    }
    if (sim->next_agents) {
        free(sim->next_agents);
        sim->next_agents = NULL;
    }
    spatial_grid_free(&sim->grid);
}

static inline void update_single_agent(const Simulation *sim,
                                       int i,
                                       uint64_t *out_checks,
                                       uint64_t *out_neighbors)
{
    const Agent *curr = &sim->current_agents[i];

    /* 1. Goal-seeking acceleration */
    double goal_fx = 0.0, goal_fy = 0.0;
    agent_compute_goal_force(curr, &goal_fx, &goal_fy);

    /* 2. Obstacle avoidance acceleration */
    double obs_fx = 0.0, obs_fy = 0.0;
    obstacle_compute_force(&sim->obstacles, curr->x, curr->y, curr->radius, &obs_fx, &obs_fy);

    /* 3. Social separation / local collision avoidance acceleration */
    double agent_fx = 0.0, agent_fy = 0.0;
    if (sim->neighbor_mode == NEIGHBORS_GRID) {
        spatial_grid_query_and_force(&sim->grid,
                                     sim->current_agents,
                                     i,
                                     sim->interact_radius,
                                     &agent_fx,
                                     &agent_fy,
                                     out_checks,
                                     out_neighbors);
    } else {
        brute_force_query_and_force(sim->current_agents,
                                    sim->num_agents,
                                    i,
                                    sim->interact_radius,
                                    &agent_fx,
                                    &agent_fy,
                                    out_checks,
                                    out_neighbors);
    }

    /* Net acceleration */
    double total_fx = goal_fx + obs_fx + agent_fx;
    double total_fy = goal_fy + obs_fy + agent_fy;

    /* Integrate state into next_agents (two-phase commit buffer) */
    agent_integrate(&sim->next_agents[i], curr, total_fx, total_fy, sim->dt);

    /* Hard penetration resolution against physical obstacles */
    obstacle_resolve_penetration(&sim->obstacles,
                                 &sim->next_agents[i].x,
                                 &sim->next_agents[i].y,
                                 &sim->next_agents[i].vx,
                                 &sim->next_agents[i].vy,
                                 sim->next_agents[i].radius);
}

void simulation_run(Simulation *sim)
{
    if (sim->mode == MODE_PARALLEL) {
        omp_set_num_threads(sim->num_threads);
    }

    sim->candidate_checks = 0;
    sim->actual_neighbors = 0;
    sim->grid_build_time_ms = 0.0;

    double t_start = omp_get_wtime();

    for (int step = 0; step < sim->steps; step++) {
        /* Phase 1A: Construct spatial grid (if spatial grid algorithm is selected) */
        if (sim->neighbor_mode == NEIGHBORS_GRID) {
            double g_start = omp_get_wtime();
            spatial_grid_build(&sim->grid, sim->current_agents, sim->num_agents);
            sim->grid_build_time_ms += (omp_get_wtime() - g_start) * 1000.0;
        }

        /* Phase 1B: Calculate forces and update states (Double-Buffered) */
        uint64_t step_checks = 0;
        uint64_t step_neighbors = 0;

        if (sim->mode == MODE_SEQUENTIAL) {
            for (int i = 0; i < sim->num_agents; i++) {
                update_single_agent(sim, i, &step_checks, &step_neighbors);
            }
        } else {
            /* OpenMP Parallel Execution with configurable scheduling */
            if (sim->schedule == SCHED_STATIC) {
                #pragma omp parallel for schedule(static) default(none) shared(sim) reduction(+:step_checks, step_neighbors)
                for (int i = 0; i < sim->num_agents; i++) {
                    update_single_agent(sim, i, &step_checks, &step_neighbors);
                }
            } else if (sim->schedule == SCHED_DYNAMIC) {
                #pragma omp parallel for schedule(dynamic, 64) default(none) shared(sim) reduction(+:step_checks, step_neighbors)
                for (int i = 0; i < sim->num_agents; i++) {
                    update_single_agent(sim, i, &step_checks, &step_neighbors);
                }
            } else { /* SCHED_GUIDED */
                #pragma omp parallel for schedule(guided, 64) default(none) shared(sim) reduction(+:step_checks, step_neighbors)
                for (int i = 0; i < sim->num_agents; i++) {
                    update_single_agent(sim, i, &step_checks, &step_neighbors);
                }
            }
        }

        sim->candidate_checks += step_checks;
        sim->actual_neighbors += step_neighbors;

        /* Phase 2: Commit — Atomic pointer swap (Double Buffering) */
        Agent *temp = sim->current_agents;
        sim->current_agents = sim->next_agents;
        sim->next_agents = temp;
    }

    double t_end = omp_get_wtime();
    sim->total_time_ms = (t_end - t_start) * 1000.0;
}

bool simulation_validate_match(const Simulation *sim1, const Simulation *sim2, double tolerance, int *mismatch_id)
{
    if (sim1->num_agents != sim2->num_agents) {
        if (mismatch_id) *mismatch_id = -1;
        return false;
    }

    for (int i = 0; i < sim1->num_agents; i++) {
        const Agent *a = &sim1->current_agents[i];
        const Agent *b = &sim2->current_agents[i];

        if (fabs(a->x - b->x) > tolerance ||
            fabs(a->y - b->y) > tolerance ||
            fabs(a->vx - b->vx) > tolerance ||
            fabs(a->vy - b->vy) > tolerance) {
            if (mismatch_id) *mismatch_id = i;
            return false;
        }
    }
    return true;
}

bool simulation_validate_invariants(const Simulation *sim, char *err_buf, size_t err_size)
{
    for (int i = 0; i < sim->num_agents; i++) {
        const Agent *a = &sim->current_agents[i];

        /* Check for NaN or Infinity */
        if (isnan(a->x) || isnan(a->y) || isnan(a->vx) || isnan(a->vy) ||
            isinf(a->x) || isinf(a->y) || isinf(a->vx) || isinf(a->vy)) {
            if (err_buf) {
                snprintf(err_buf, err_size, "Agent %d has non-finite state: pos=(%f, %f) vel=(%f, %f)",
                         a->id, a->x, a->y, a->vx, a->vy);
            }
            return false;
        }

        /* Check world boundaries */
        if (a->x < -0.01 || a->x > ENV_WIDTH + 0.01 ||
            a->y < -0.01 || a->y > ENV_HEIGHT + 0.01) {
            if (err_buf) {
                snprintf(err_buf, err_size, "Agent %d exceeded world boundaries: pos=(%f, %f)",
                         a->id, a->x, a->y);
            }
            return false;
        }

        /* Check obstacle non-penetration */
        if (obstacle_is_penetrating(&sim->obstacles, a->x, a->y, a->radius)) {
            if (err_buf) {
                snprintf(err_buf, err_size, "Agent %d penetrated an obstacle: pos=(%f, %f)",
                         a->id, a->x, a->y);
            }
            return false;
        }
    }
    return true;
}

void simulation_get_density(const Simulation *sim, int rows, int cols, int *density_grid)
{
    memset(density_grid, 0, (size_t)(rows * cols) * sizeof(int));

    for (int i = 0; i < sim->num_agents; i++) {
        int c = (int)(sim->current_agents[i].x / ENV_WIDTH * cols);
        int r = (int)(sim->current_agents[i].y / ENV_HEIGHT * rows);

        if (c < 0) c = 0;
        if (c >= cols) c = cols - 1;
        if (r < 0) r = 0;
        if (r >= rows) r = rows - 1;

        density_grid[r * cols + c]++;
    }
}
