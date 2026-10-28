#ifndef CROWDFLOW_SPATIAL_GRID_H
#define CROWDFLOW_SPATIAL_GRID_H

#include "common.h"
#include "agent.h"

typedef struct {
    double width;
    double height;
    double cell_size;
    int cols;
    int rows;
    int num_cells;
    int *head;           /* Array of size num_cells: head agent index per cell (-1 if empty) */
    int *next;           /* Array of size num_agents: next agent in the cell list */
    int max_agents;
} SpatialGrid;

/* Initialise the spatial grid data structures */
bool spatial_grid_init(SpatialGrid *grid, double width, double height, double cell_size, int max_agents);

/* Release spatial grid heap allocations */
void spatial_grid_free(SpatialGrid *grid);

/* Construct the spatial grid from current agent positions (O(N) operation) */
void spatial_grid_build(SpatialGrid *grid, const Agent *agents, int num_agents);

/* Map (x, y) coordinates to cell indices (clamped within valid range) */
void spatial_grid_coord_to_cell(const SpatialGrid *grid, double x, double y, int *cell_x, int *cell_y);

/* Query neighbors of agent 'i' using the spatial grid (O(1) average complexity) */
void spatial_grid_query_and_force(const SpatialGrid *grid,
                                  const Agent *agents,
                                  int agent_idx,
                                  double interact_radius,
                                  double *fx,
                                  double *fy,
                                  uint64_t *candidate_checks,
                                  uint64_t *actual_neighbors);

/* Reference O(N) brute-force neighbor query for verification & baseline comparison */
void brute_force_query_and_force(const Agent *agents,
                                 int num_agents,
                                 int agent_idx,
                                 double interact_radius,
                                 double *fx,
                                 double *fy,
                                 uint64_t *candidate_checks,
                                 uint64_t *actual_neighbors);

#endif /* CROWDFLOW_SPATIAL_GRID_H */
