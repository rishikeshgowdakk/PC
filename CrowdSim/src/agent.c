#include "agent.h"

/* Fast, 100% portable 64-bit Linear Congruential Generator for reproducible PRNG */
static uint64_t lcg_state = 123456789ULL;

static void lcg_seed(uint64_t seed)
{
    lcg_state = (seed == 0) ? 123456789ULL : seed;
}

static double lcg_rand_double(void)
{
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(lcg_state >> 32) / (double)0xFFFFFFFFULL;
}

void agent_init(Agent *a, int id, double x, double y, double gx, double gy, double pref_speed, double radius)
{
    a->id = id;
    a->x = x;
    a->y = y;
    a->vx = 0.0;
    a->vy = 0.0;
    a->gx = gx;
    a->gy = gy;
    a->pref_speed = pref_speed;
    a->radius = radius;
    a->max_accel = DEFAULT_MAX_ACCEL;
}

void agent_init_population(Agent *agents, int count, ScenarioType scenario, unsigned int seed)
{
    lcg_seed((uint64_t)seed);

    for (int i = 0; i < count; i++) {
        double x, y, gx, gy;
        double speed = DEFAULT_PREF_SPEED + (lcg_rand_double() - 0.5) * 0.4; /* 1.1 - 1.5 m/s */
        double radius = DEFAULT_AGENT_RADIUS;

        if (scenario == SCENARIO_BOTTLENECK) {
            /* Agents start on left half [5, 45], target is right exit [85, 95] */
            x = 5.0 + lcg_rand_double() * 38.0;
            y = 5.0 + lcg_rand_double() * (ENV_HEIGHT - 10.0);
            gx = 90.0 + lcg_rand_double() * 5.0;
            gy = (ENV_HEIGHT * 0.5) + (lcg_rand_double() - 0.5) * 6.0;
        } else {
            /* Bidirectional crossing crowd: Even IDs flow Left->Right, Odd IDs Right->Left */
            if (i % 2 == 0) {
                x = 5.0 + lcg_rand_double() * 25.0;
                y = 5.0 + lcg_rand_double() * (ENV_HEIGHT - 10.0);
                gx = ENV_WIDTH - 5.0;
                gy = y + (lcg_rand_double() - 0.5) * 10.0;
            } else {
                x = ENV_WIDTH - 30.0 + lcg_rand_double() * 25.0;
                y = 5.0 + lcg_rand_double() * (ENV_HEIGHT - 10.0);
                gx = 5.0;
                gy = y + (lcg_rand_double() - 0.5) * 10.0;
            }
        }

        /* Clamp goal within world boundaries */
        if (gx < 2.0) gx = 2.0;
        if (gx > ENV_WIDTH - 2.0) gx = ENV_WIDTH - 2.0;
        if (gy < 2.0) gy = 2.0;
        if (gy > ENV_HEIGHT - 2.0) gy = ENV_HEIGHT - 2.0;

        agent_init(&agents[i], i, x, y, gx, gy, speed, radius);
    }
}

void agent_compute_goal_force(const Agent *a, double *fx, double *fy)
{
    double dx = a->gx - a->x;
    double dy = a->gy - a->y;
    double dist = sqrt(dx * dx + dy * dy);

    if (dist < 0.2) {
        /* Already reached goal: damp velocity smoothly to zero */
        *fx = -a->vx / RELAXATION_TIME;
        *fy = -a->vy / RELAXATION_TIME;
        return;
    }

    /* Unit direction toward goal */
    double nx = dx / dist;
    double ny = dy / dist;

    /* Desired velocity vector */
    double v_des_x = nx * a->pref_speed;
    double v_des_y = ny * a->pref_speed;

    /* Steering acceleration (relaxation toward desired velocity) */
    *fx = (v_des_x - a->vx) / RELAXATION_TIME;
    *fy = (v_des_y - a->vy) / RELAXATION_TIME;
}

void agent_compute_agent_repulsion(const Agent *self, const Agent *other, double *fx, double *fy, double interact_radius)
{
    double dx = self->x - other->x;
    double dy = self->y - other->y;
    double dist_sq = dx * dx + dy * dy;

    double r_interact_sq = interact_radius * interact_radius;
    if (dist_sq >= r_interact_sq || dist_sq < 1e-12) {
        *fx = 0.0;
        *fy = 0.0;
        return;
    }

    double dist = sqrt(dist_sq);
    double nx = dx / dist;
    double ny = dy / dist;

    /* Combined radii of both agents */
    double combined_radius = self->radius + other->radius;
    double overlap = combined_radius - dist;

    double rep_magnitude = 0.0;
    if (overlap > 0.0) {
        /* Hard contact / strong physical repulsion */
        rep_magnitude = REPULSE_STRENGTH * 2.0 * (1.0 + overlap * 5.0);
    } else {
        /* Smooth social distance decay */
        double distance_gap = dist - combined_radius;
        rep_magnitude = REPULSE_STRENGTH * exp(-distance_gap / REPULSE_FALLOFF);
    }

    *fx = nx * rep_magnitude;
    *fy = ny * rep_magnitude;
}

void agent_integrate(Agent *dest, const Agent *src, double fx, double fy, double dt)
{
    dest->id = src->id;
    dest->gx = src->gx;
    dest->gy = src->gy;
    dest->pref_speed = src->pref_speed;
    dest->radius = src->radius;
    dest->max_accel = src->max_accel;

    /* Limit maximum acceleration */
    double accel_mag = sqrt(fx * fx + fy * fy);
    if (accel_mag > src->max_accel) {
        fx = (fx / accel_mag) * src->max_accel;
        fy = (fy / accel_mag) * src->max_accel;
    }

    /* Update velocity: v = v0 + a * dt */
    double new_vx = src->vx + fx * dt;
    double new_vy = src->vy + fy * dt;

    /* Clamp maximum speed */
    double new_speed = sqrt(new_vx * new_vx + new_vy * new_vy);
    if (new_speed > DEFAULT_MAX_SPEED) {
        new_vx = (new_vx / new_speed) * DEFAULT_MAX_SPEED;
        new_vy = (new_vy / new_speed) * DEFAULT_MAX_SPEED;
    }

    /* Update position: x = x0 + v * dt */
    double new_x = src->x + new_vx * dt;
    double new_y = src->y + new_vy * dt;

    /* World boundary reflection / clamping */
    double min_x = src->radius;
    double max_x = ENV_WIDTH - src->radius;
    double min_y = src->radius;
    double max_y = ENV_HEIGHT - src->radius;

    if (new_x < min_x) {
        new_x = min_x;
        new_vx = -new_vx * 0.5; /* Inelastic boundary bounce */
    } else if (new_x > max_x) {
        new_x = max_x;
        new_vx = -new_vx * 0.5;
    }

    if (new_y < min_y) {
        new_y = min_y;
        new_vy = -new_vy * 0.5;
    } else if (new_y > max_y) {
        new_y = max_y;
        new_vy = -new_vy * 0.5;
    }

    dest->x = new_x;
    dest->y = new_y;
    dest->vx = new_vx;
    dest->vy = new_vy;
}
