#ifndef CROWDFLOW_COMMON_H
#define CROWDFLOW_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <omp.h>

#define ENV_WIDTH               100.0
#define ENV_HEIGHT               60.0
#define DEFAULT_DT                0.05   /* 50 ms time step */
#define DEFAULT_AGENT_RADIUS      0.4    /* 0.4 m body radius */
#define DEFAULT_PREF_SPEED        1.3    /* 1.3 m/s human walking speed */
#define DEFAULT_INTERACT_RADIUS   2.0    /* 2.0 m interaction distance */
#define DEFAULT_MAX_ACCEL         5.0    /* Max acceleration m/s^2 */
#define DEFAULT_MAX_SPEED         2.5    /* Max walking/running speed */
#define RELAXATION_TIME           0.5    /* Tau for goal steering (seconds) */

/* Force model parameters */
#define REPULSE_STRENGTH         12.0    /* Peak social repulsion force */
#define REPULSE_FALLOFF           0.8    /* Spatial decay range */
#define OBSTACLE_STRENGTH        20.0    /* Obstacle avoidance force */
#define OBSTACLE_MARGIN           0.8    /* Buffer distance for obstacle buffer */

/* Validation tolerance for floating point comparisons */
#define TOLERANCE_DEFAULT        1e-6

/* Limits */
#define MAX_AGENTS            5000000
#define MAX_STEPS             1000000
#define MAX_THREADS              1024

/* Execution mode */
typedef enum {
    MODE_PARALLEL = 0,
    MODE_SEQUENTIAL = 1
} ExecMode;

/* Neighbor detection algorithm */
typedef enum {
    NEIGHBORS_GRID = 0,
    NEIGHBORS_BRUTE_FORCE = 1
} NeighborMode;

/* OpenMP scheduling policy */
typedef enum {
    SCHED_STATIC = 0,
    SCHED_DYNAMIC = 1,
    SCHED_GUIDED = 2
} SchedPolicy;

/* Scenario types */
typedef enum {
    SCENARIO_EMPTY = 0,
    SCENARIO_PILLARS = 1,
    SCENARIO_BOTTLENECK = 2
} ScenarioType;

#endif /* CROWDFLOW_COMMON_H */
