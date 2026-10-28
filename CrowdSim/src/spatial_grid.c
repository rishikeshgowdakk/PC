#include "spatial_grid.h"

bool spatial_grid_init(SpatialGrid *grid, double width, double height, double cell_size, int max_agents)
{
    grid->width = width;
    grid->height = height;
    grid->cell_size = cell_size;
    grid->max_agents = max_agents;

    grid->cols = (int)ceil(width / cell_size);
    grid->rows = (int)ceil(height / cell_size);
    if (grid->cols < 1) grid->cols = 1;
    if (grid->rows < 1) grid->rows = 1;
    grid->num_cells = grid->cols * grid->rows;

    grid->head = (int *)malloc((size_t)grid->num_cells * sizeof(int));
    grid->next = (int *)malloc((size_t)max_agents * sizeof(int));

    if (!grid->head || !grid->next) {
        free(grid->head);
        free(grid->next);
        grid->head = NULL;
        grid->next = NULL;
        return false;
    }

    memset(grid->head, 0xFF, (size_t)grid->num_cells * sizeof(int));
    memset(grid->next, 0xFF, (size_t)max_agents * sizeof(int));
    return true;
}

void spatial_grid_free(SpatialGrid *grid)
{
    if (grid->head) {
        free(grid->head);
        grid->head = NULL;
    }
    if (grid->next) {
        free(grid->next);
        grid->next = NULL;
    }
    grid->num_cells = 0;
}

void spatial_grid_coord_to_cell(const SpatialGrid *grid, double x, double y, int *cell_x, int *cell_y)
{
    int cx = (int)(x / grid->cell_size);
    int cy = (int)(y / grid->cell_size);

    if (cx < 0) cx = 0;
    if (cx >= grid->cols) cx = grid->cols - 1;
    if (cy < 0) cy = 0;
    if (cy >= grid->rows) cy = grid->rows - 1;

    *cell_x = cx;
    *cell_y = cy;
}

void spatial_grid_build(SpatialGrid *grid, const Agent *agents, int num_agents)
{
    /* Reset all cell heads to -1 */
    memset(grid->head, 0xFF, (size_t)grid->num_cells * sizeof(int));

    /* Populate linked lists: O(N) single-pass insertion */
    for (int i = 0; i < num_agents; i++) {
        int cx, cy;
        spatial_grid_coord_to_cell(grid, agents[i].x, agents[i].y, &cx, &cy);
        int cell_id = cy * grid->cols + cx;

        grid->next[i] = grid->head[cell_id];
        grid->head[cell_id] = i;
    }
}

void spatial_grid_query_and_force(const SpatialGrid *grid,
                                  const Agent *agents,
                                  int agent_idx,
                                  double interact_radius,
                                  double *fx,
                                  double *fy,
                                  uint64_t *candidate_checks,
                                  uint64_t *actual_neighbors)
{
    const Agent *self = &agents[agent_idx];
    int cx, cy;
    spatial_grid_coord_to_cell(grid, self->x, self->y, &cx, &cy);

    double total_fx = 0.0;
    double total_fy = 0.0;
    uint64_t checks = 0;
    uint64_t neighbors = 0;

    /* Scan the 9 adjacent cells (3x3 neighborhood) */
    for (int dy = -1; dy <= 1; dy++) {
        int ny = cy + dy;
        if (ny < 0 || ny >= grid->rows) continue;

        for (int dx = -1; dx <= 1; dx++) {
            int nx = cx + dx;
            if (nx < 0 || nx >= grid->cols) continue;

            int cell_id = ny * grid->cols + nx;
            int curr = grid->head[cell_id];

            while (curr != -1) {
                if (curr != agent_idx) {
                    checks++;
                    double r_fx, r_fy;
                    agent_compute_agent_repulsion(self, &agents[curr], &r_fx, &r_fy, interact_radius);
                    if (r_fx != 0.0 || r_fy != 0.0) {
                        total_fx += r_fx;
                        total_fy += r_fy;
                        neighbors++;
                    }
                }
                curr = grid->next[curr];
            }
        }
    }

    *fx = total_fx;
    *fy = total_fy;
    *candidate_checks += checks;
    *actual_neighbors += neighbors;
}

void brute_force_query_and_force(const Agent *agents,
                                 int num_agents,
                                 int agent_idx,
                                 double interact_radius,
                                 double *fx,
                                 double *fy,
                                 uint64_t *candidate_checks,
                                 uint64_t *actual_neighbors)
{
    const Agent *self = &agents[agent_idx];
    double total_fx = 0.0;
    double total_fy = 0.0;
    uint64_t checks = 0;
    uint64_t neighbors = 0;

    for (int j = 0; j < num_agents; j++) {
        if (j != agent_idx) {
            checks++;
            double r_fx, r_fy;
            agent_compute_agent_repulsion(self, &agents[j], &r_fx, &r_fy, interact_radius);
            if (r_fx != 0.0 || r_fy != 0.0) {
                total_fx += r_fx;
                total_fy += r_fy;
                neighbors++;
            }
        }
    }

    *fx = total_fx;
    *fy = total_fy;
    *candidate_checks += checks;
    *actual_neighbors += neighbors;
}
