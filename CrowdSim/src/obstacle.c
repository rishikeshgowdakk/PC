#include "obstacle.h"

void obstacle_init_set(ObstacleSet *set, ScenarioType scenario)
{
    set->count = 0;

    if (scenario == SCENARIO_PILLARS) {
        /* 4 circular pillars arranged symmetrically in the central area */
        set->count = 4;
        
        /* Pillar 1 */
        set->obstacles[0].type = OBSTACLE_CIRCLE;
        set->obstacles[0].cx = 40.0;
        set->obstacles[0].cy = 20.0;
        set->obstacles[0].radius = 4.0;

        /* Pillar 2 */
        set->obstacles[1].type = OBSTACLE_CIRCLE;
        set->obstacles[1].cx = 40.0;
        set->obstacles[1].cy = 40.0;
        set->obstacles[1].radius = 4.0;

        /* Pillar 3 */
        set->obstacles[2].type = OBSTACLE_CIRCLE;
        set->obstacles[2].cx = 60.0;
        set->obstacles[2].cy = 20.0;
        set->obstacles[2].radius = 4.0;

        /* Pillar 4 */
        set->obstacles[3].type = OBSTACLE_CIRCLE;
        set->obstacles[3].cx = 60.0;
        set->obstacles[3].cy = 40.0;
        set->obstacles[3].radius = 4.0;

    } else if (scenario == SCENARIO_BOTTLENECK) {
        /* A central dividing wall at x = 50.0 with a doorway in the middle y in [24, 36] */
        set->count = 2;

        /* Top wall: [48.0, 0.0] to [52.0, 24.0] */
        set->obstacles[0].type = OBSTACLE_AABB;
        set->obstacles[0].min_x = 48.0;
        set->obstacles[0].min_y = 0.0;
        set->obstacles[0].max_x = 52.0;
        set->obstacles[0].max_y = 24.0;

        /* Bottom wall: [48.0, 36.0] to [52.0, 60.0] */
        set->obstacles[1].type = OBSTACLE_AABB;
        set->obstacles[1].min_x = 48.0;
        set->obstacles[1].min_y = 36.0;
        set->obstacles[1].max_x = 52.0;
        set->obstacles[1].max_y = 60.0;
    }
}

void obstacle_compute_force(const ObstacleSet *set, double x, double y, double agent_radius, double *fx, double *fy)
{
    double total_fx = 0.0;
    double total_fy = 0.0;

    for (int i = 0; i < set->count; i++) {
        const Obstacle *obs = &set->obstacles[i];

        if (obs->type == OBSTACLE_CIRCLE) {
            double dx = x - obs->cx;
            double dy = y - obs->cy;
            double dist = sqrt(dx * dx + dy * dy);

            if (dist < 1e-9) {
                total_fx += OBSTACLE_STRENGTH;
                continue;
            }

            double effective_bound = obs->radius + agent_radius + OBSTACLE_MARGIN;
            if (dist < effective_bound) {
                double nx = dx / dist;
                double ny = dy / dist;
                double overlap = effective_bound - dist;
                double mag = OBSTACLE_STRENGTH * (overlap / OBSTACLE_MARGIN);
                total_fx += nx * mag;
                total_fy += ny * mag;
            }
        } else if (obs->type == OBSTACLE_AABB) {
            /* Clamp (x, y) to closest point on AABB rectangle */
            double qx = (x < obs->min_x) ? obs->min_x : ((x > obs->max_x) ? obs->max_x : x);
            double qy = (y < obs->min_y) ? obs->min_y : ((y > obs->max_y) ? obs->max_y : y);

            double dx = x - qx;
            double dy = y - qy;
            double dist = sqrt(dx * dx + dy * dy);

            if (dist > 1e-9) {
                double effective_bound = agent_radius + OBSTACLE_MARGIN;
                if (dist < effective_bound) {
                    double nx = dx / dist;
                    double ny = dy / dist;
                    double overlap = effective_bound - dist;
                    double mag = OBSTACLE_STRENGTH * (overlap / OBSTACLE_MARGIN);
                    total_fx += nx * mag;
                    total_fy += ny * mag;
                }
            } else {
                /* Agent is inside AABB: push out toward closest edge */
                double d_left = x - obs->min_x;
                double d_right = obs->max_x - x;
                double d_bottom = y - obs->min_y;
                double d_top = obs->max_y - y;

                double min_dist = d_left;
                double push_x = -1.0, push_y = 0.0;

                if (d_right < min_dist) { min_dist = d_right; push_x = 1.0; push_y = 0.0; }
                if (d_bottom < min_dist) { min_dist = d_bottom; push_x = 0.0; push_y = -1.0; }
                if (d_top < min_dist) { min_dist = d_top; push_x = 0.0; push_y = 1.0; }

                total_fx += push_x * OBSTACLE_STRENGTH * 2.0;
                total_fy += push_y * OBSTACLE_STRENGTH * 2.0;
            }
        }
    }

    *fx = total_fx;
    *fy = total_fy;
}

void obstacle_resolve_penetration(const ObstacleSet *set, double *x, double *y, double *vx, double *vy, double agent_radius)
{
    for (int i = 0; i < set->count; i++) {
        const Obstacle *obs = &set->obstacles[i];

        if (obs->type == OBSTACLE_CIRCLE) {
            double dx = *x - obs->cx;
            double dy = *y - obs->cy;
            double dist = sqrt(dx * dx + dy * dy);
            double min_dist = obs->radius + agent_radius;

            if (dist < min_dist) {
                double nx = (dist > 1e-9) ? (dx / dist) : 1.0;
                double ny = (dist > 1e-9) ? (dy / dist) : 0.0;

                /* Project to circle perimeter */
                *x = obs->cx + nx * min_dist;
                *y = obs->cy + ny * min_dist;

                /* Cancel velocity into the obstacle */
                double v_dot_n = (*vx) * nx + (*vy) * ny;
                if (v_dot_n < 0.0) {
                    *vx -= v_dot_n * nx;
                    *vy -= v_dot_n * ny;
                }
            }
        } else if (obs->type == OBSTACLE_AABB) {
            /* Check if within expanded AABB */
            double exp_min_x = obs->min_x - agent_radius;
            double exp_max_x = obs->max_x + agent_radius;
            double exp_min_y = obs->min_y - agent_radius;
            double exp_max_y = obs->max_y + agent_radius;

            if (*x >= exp_min_x && *x <= exp_max_x && *y >= exp_min_y && *y <= exp_max_y) {
                /* Push out to nearest side */
                double d_left = fabs(*x - exp_min_x);
                double d_right = fabs(*x - exp_max_x);
                double d_bottom = fabs(*y - exp_min_y);
                double d_top = fabs(*y - exp_max_y);

                double min_d = d_left;
                int side = 0; /* 0: left, 1: right, 2: bottom, 3: top */

                if (d_right < min_d) { min_d = d_right; side = 1; }
                if (d_bottom < min_d) { min_d = d_bottom; side = 2; }
                if (d_top < min_d) { min_d = d_top; side = 3; }

                if (side == 0) { *x = exp_min_x; if (*vx > 0.0) *vx = 0.0; }
                else if (side == 1) { *x = exp_max_x; if (*vx < 0.0) *vx = 0.0; }
                else if (side == 2) { *y = exp_min_y; if (*vy > 0.0) *vy = 0.0; }
                else if (side == 3) { *y = exp_max_y; if (*vy < 0.0) *vy = 0.0; }
            }
        }
    }
}

bool obstacle_is_penetrating(const ObstacleSet *set, double x, double y, double agent_radius)
{
    for (int i = 0; i < set->count; i++) {
        const Obstacle *obs = &set->obstacles[i];

        if (obs->type == OBSTACLE_CIRCLE) {
            double dx = x - obs->cx;
            double dy = y - obs->cy;
            double dist = sqrt(dx * dx + dy * dy);
            if (dist < (obs->radius + agent_radius - 1e-4)) {
                return true;
            }
        } else if (obs->type == OBSTACLE_AABB) {
            if (x > (obs->min_x - agent_radius + 1e-4) &&
                x < (obs->max_x + agent_radius - 1e-4) &&
                y > (obs->min_y - agent_radius + 1e-4) &&
                y < (obs->max_y + agent_radius - 1e-4)) {
                return true;
            }
        }
    }
    return false;
}
