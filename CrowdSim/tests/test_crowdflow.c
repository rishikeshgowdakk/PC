#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "../src/common.h"
#include "../src/agent.h"
#include "../src/obstacle.h"
#include "../src/spatial_grid.h"
#include "../src/simulation.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d: %s)\n", msg, __LINE__, #cond); \
    } \
} while(0)

static void test_agent_init_and_seed(void)
{
    printf("\n=== Test Suite 1: Agent Initialization & Seed Determinism ===\n");
    Agent a1[100], a2[100], a3[100];

    agent_init_population(a1, 100, SCENARIO_PILLARS, 42);
    agent_init_population(a2, 100, SCENARIO_PILLARS, 42);
    agent_init_population(a3, 100, SCENARIO_PILLARS, 999);

    bool match_identical = true;
    for (int i = 0; i < 100; i++) {
        if (a1[i].x != a2[i].x || a1[i].y != a2[i].y ||
            a1[i].gx != a2[i].gx || a1[i].gy != a2[i].gy) {
            match_identical = false;
            break;
        }
    }
    TEST_ASSERT(match_identical, "Identical seeds produce bitwise identical populations");

    bool match_diff = false;
    for (int i = 0; i < 100; i++) {
        if (a1[i].x != a3[i].x || a1[i].y != a3[i].y) {
            match_diff = true;
            break;
        }
    }
    TEST_ASSERT(match_diff, "Different seeds produce distinct populations");
}

static void test_goal_steering(void)
{
    printf("\n=== Test Suite 2: Goal Steering Dynamics ===\n");
    Agent a;
    agent_init(&a, 0, 10.0, 10.0, 20.0, 10.0, 1.3, 0.4);

    double fx, fy;
    agent_compute_goal_force(&a, &fx, &fy);

    TEST_ASSERT(fx > 0.0 && fabs(fy) < 1e-6, "Steering force directs toward goal along +X axis");

    /* Test arrival damping */
    Agent arr;
    agent_init(&arr, 1, 19.95, 10.0, 20.0, 10.0, 1.3, 0.4);
    arr.vx = 1.0;
    arr.vy = 0.0;
    agent_compute_goal_force(&arr, &fx, &fy);
    TEST_ASSERT(fx < 0.0, "Near goal, steering force damps remaining velocity");
}

static void test_obstacle_mechanics(void)
{
    printf("\n=== Test Suite 3: Obstacle Avoidance & Hard Resolution ===\n");
    ObstacleSet obs;
    obstacle_init_set(&obs, SCENARIO_PILLARS);

    /* Pillar 0 is at cx=40.0, cy=20.0, radius=4.0 */
    /* Agent at x=35.0, y=20.0 approaches from left (distance 5.0, bound = 4.0 + 0.4 + 0.8 = 5.2) */
    double fx, fy;
    obstacle_compute_force(&obs, 35.0, 20.0, 0.4, &fx, &fy);
    TEST_ASSERT(fx < 0.0 && fabs(fy) < 1e-6, "Circular pillar repels approaching agent outward");

    /* Test hard penetration resolution */
    double x = 41.0, y = 20.0, vx = 1.0, vy = 0.0; /* Inside pillar */
    obstacle_resolve_penetration(&obs, &x, &y, &vx, &vy, 0.4);
    double dist_center = sqrt((x - 40.0)*(x - 40.0) + (y - 20.0)*(y - 20.0));
    TEST_ASSERT(dist_center >= (4.0 + 0.4 - 1e-6), "Penetrated agent projected out to perimeter");
}

static void test_grid_vs_brute_force(void)
{
    printf("\n=== Test Suite 4: Spatial Grid vs Brute Force Equivalence ===\n");
    int n = 500;
    Agent agents[500];
    agent_init_population(agents, n, SCENARIO_PILLARS, 1234);

    SpatialGrid grid;
    spatial_grid_init(&grid, ENV_WIDTH, ENV_HEIGHT, DEFAULT_INTERACT_RADIUS, n);
    spatial_grid_build(&grid, agents, n);

    bool forces_match = true;
    bool counts_match = true;
    uint64_t grid_checks = 0, grid_neighbors = 0;
    uint64_t brute_checks = 0, brute_neighbors = 0;

    for (int i = 0; i < n; i++) {
        double g_fx = 0.0, g_fy = 0.0;
        double b_fx = 0.0, b_fy = 0.0;

        spatial_grid_query_and_force(&grid, agents, i, DEFAULT_INTERACT_RADIUS, &g_fx, &g_fy, &grid_checks, &grid_neighbors);
        brute_force_query_and_force(agents, n, i, DEFAULT_INTERACT_RADIUS, &b_fx, &b_fy, &brute_checks, &brute_neighbors);

        if (fabs(g_fx - b_fx) > 1e-6 || fabs(g_fy - b_fy) > 1e-6) {
            forces_match = false;
        }
    }

    if (grid_neighbors != brute_neighbors) {
        counts_match = false;
    }

    TEST_ASSERT(forces_match, "Spatial grid repulsive forces match brute force within 1e-6");
    TEST_ASSERT(counts_match, "Spatial grid detected identical total neighbor count as brute force");
    TEST_ASSERT(grid_checks < brute_checks, "Spatial grid executed substantially fewer candidate checks");

    printf("    -> Brute Force candidate checks: %lu\n", (unsigned long)brute_checks);
    printf("    -> Spatial Grid candidate checks: %lu (Reduction: %.1f%%)\n",
           (unsigned long)grid_checks,
           (1.0 - (double)grid_checks / (double)brute_checks) * 100.0);

    spatial_grid_free(&grid);
}

static void test_sequential_parallel_equivalence(void)
{
    printf("\n=== Test Suite 5: Sequential vs OpenMP Parallel Equivalence ===\n");
    int agents = 1000;
    int steps = 40;

    Simulation seq_sim;
    simulation_init(&seq_sim, agents, steps, DEFAULT_DT, 1, MODE_SEQUENTIAL, NEIGHBORS_GRID, SCHED_STATIC, SCENARIO_PILLARS, 42);
    simulation_run(&seq_sim);

    int thread_cases[] = {1, 2, 4, 8};
    for (size_t t = 0; t < sizeof(thread_cases)/sizeof(thread_cases[0]); t++) {
        int threads = thread_cases[t];
        Simulation par_sim;
        simulation_init(&par_sim, agents, steps, DEFAULT_DT, threads, MODE_PARALLEL, NEIGHBORS_GRID, SCHED_STATIC, SCENARIO_PILLARS, 42);
        simulation_run(&par_sim);

        int mismatch_id = -1;
        bool match = simulation_validate_match(&seq_sim, &par_sim, TOLERANCE_DEFAULT, &mismatch_id);

        char label[128];
        snprintf(label, sizeof(label), "Sequential matches OpenMP with %d thread(s) (tol <= 1e-6)", threads);
        TEST_ASSERT(match, label);

        char err_buf[256];
        bool invariants = simulation_validate_invariants(&par_sim, err_buf, sizeof(err_buf));
        TEST_ASSERT(invariants, "Simulation invariants (bounds, no NaN, no obstacle overlap) satisfied");

        simulation_free(&par_sim);
    }

    simulation_free(&seq_sim);
}

int main(void)
{
    printf("========================================================\n");
    printf("        CrowdFlow Comprehensive Test Suite\n");
    printf("========================================================\n");

    test_agent_init_and_seed();
    test_goal_steering();
    test_obstacle_mechanics();
    test_grid_vs_brute_force();
    test_sequential_parallel_equivalence();

    printf("\n========================================================\n");
    printf(" Test Results: %d / %d Passed (%.1f%%)\n",
           g_tests_passed, g_tests_run, (double)g_tests_passed / (double)g_tests_run * 100.0);
    printf("========================================================\n\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
