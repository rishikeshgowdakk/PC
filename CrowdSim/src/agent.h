#ifndef CROWDFLOW_AGENT_H
#define CROWDFLOW_AGENT_H

#include "common.h"

typedef struct {
    int id;               /* Unique stable identifier */
    double x, y;          /* Current 2D position (meters) */
    double vx, vy;        /* Current 2D velocity (m/s) */
    double gx, gy;        /* Destination / goal target (meters) */
    double pref_speed;    /* Desired walking speed */
    double radius;        /* Personal body radius */
    double max_accel;     /* Maximum allowed acceleration */
} Agent;

/* Initialize a single agent */
void agent_init(Agent *a, int id, double x, double y, double gx, double gy, double pref_speed, double radius);

/* Deterministically generate initial crowd configuration with repeatable seed */
void agent_init_population(Agent *agents, int count, ScenarioType scenario, unsigned int seed);

/* Compute goal-seeking steering acceleration toward destination */
void agent_compute_goal_force(const Agent *a, double *fx, double *fy);

/* Compute repulsive separation force exerted on 'self' by 'other' */
void agent_compute_agent_repulsion(const Agent *self, const Agent *other, double *fx, double *fy, double interact_radius);

/* Integrate acceleration over dt into next state with velocity clamping */
void agent_integrate(Agent *dest, const Agent *src, double fx, double fy, double dt);

#endif /* CROWDFLOW_AGENT_H */
