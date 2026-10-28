#ifndef CROWDFLOW_OBSTACLE_H
#define CROWDFLOW_OBSTACLE_H

#include "common.h"

typedef enum {
    OBSTACLE_CIRCLE = 0,
    OBSTACLE_AABB = 1
} ObstacleType;

typedef struct {
    ObstacleType type;
    /* Circle parameters */
    double cx, cy;
    double radius;
    /* AABB parameters: [min_x, min_y] to [max_x, max_y] */
    double min_x, min_y;
    double max_x, max_y;
} Obstacle;

#define MAX_OBSTACLES 16

typedef struct {
    int count;
    Obstacle obstacles[MAX_OBSTACLES];
} ObstacleSet;

/* Initialise an obstacle set according to scenario */
void obstacle_init_set(ObstacleSet *set, ScenarioType scenario);

/* Compute cumulative repulsive avoidance force from all obstacles */
void obstacle_compute_force(const ObstacleSet *set, double x, double y, double agent_radius, double *fx, double *fy);

/* Hard resolution / projection if an agent penetrates an obstacle */
void obstacle_resolve_penetration(const ObstacleSet *set, double *x, double *y, double *vx, double *vy, double agent_radius);

/* Check if an agent position violates/penetrates any obstacle */
bool obstacle_is_penetrating(const ObstacleSet *set, double x, double y, double agent_radius);

#endif /* CROWDFLOW_OBSTACLE_H */
