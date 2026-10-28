# CrowdFlow Architecture Specification

## 1. Architectural Philosophy

**CrowdFlow** is engineered with a strict separation of concerns across three distinct layers:
1. **Computational Core (Native ISO C11 + OpenMP):** High-throughput, multi-threaded simulation engine operating on contiguous memory buffers.
2. **Integration & Validation Layer (Python Standard Library):** Process lifecycle manager, argument sanitizer, and JSON-RPC API controller.
3. **Presentation & Analytics Layer (HTML5 / Canvas / Vanilla JavaScript):** Client-side visualization dashboard rendering real simulation data and scaling charts.

---

## 2. Component Decomposition

```
CrowdFlow/
├── src/
│   ├── common.h         # System-wide physical constants, limits, enumerations
│   ├── agent.h/.c       # Agent kinematics, goal steering, social force calculations
│   ├── obstacle.h/.c    # Circular & AABB obstacle collision resolution
│   ├── spatial_grid.h/.c# Uniform spatial partitioning grid (Forward-Star linked lists)
│   ├── simulation.h/.c  # Two-phase commit orchestrator (Sequential & OpenMP)
│   ├── benchmark.h/.c   # Statistical timing engine (mean, median, stddev, sweeps)
│   └── main.c           # CLI interface supporting CSV, JSON, and compare modes
├── tests/
│   └── test_crowdflow.c # Unit & integration regression test harness
├── server.py            # Local loopback HTTP API controller
└── web/
    ├── index.html       # Semantic dashboard interface
    ├── style.css        # Responsive CSS Grid & KPI cards
    └── app.js           # Canvas renderer, state manager, CSV exporter
```

---

## 3. Data Structures & Memory Layout

### 3.1 Agent Representation (`src/agent.h`)
```c
typedef struct {
    int id;               /* Unique stable identifier (4 bytes) */
    double x, y;          /* 2D world coordinates (16 bytes) */
    double vx, vy;        /* 2D velocity vector (16 bytes) */
    double gx, gy;        /* Target goal coordinates (16 bytes) */
    double pref_speed;    /* Desired walking speed (8 bytes) */
    double radius;        /* Personal space boundary radius (8 bytes) */
    double max_accel;     /* Maximum allowed acceleration (8 bytes) */
} Agent;
```
Each `Agent` struct occupies 76 bytes, aligned to 80 bytes in contiguous arrays. Memory for the entire population is allocated as a single continuous block on the heap, ensuring hardware prefetcher efficiency and high cache-line hit rates.

### 3.2 Two-Phase Double Buffering (`current_agents` $\longleftrightarrow$ `next_agents`)
To eliminate data races during multi-threaded force calculation without expensive synchronization locks:
- During step $t$: All threads read exclusively from `sim->current_agents`.
- Thread $k$ calculates the total net force on agent $i$ and writes the updated kinematic state to `sim->next_agents[i]`.
- No thread modifies `current_agents`. No thread reads `next_agents`.
- At the end of step $t$, the pointers are swapped in $\mathcal{O}(1)$ time:
  ```c
  Agent *temp = sim->current_agents;
  sim->current_agents = sim->next_agents;
  sim->next_agents = temp;
  ```
This guarantees **race-free determinism** across arbitrary thread allocations.

### 3.3 Spatial Grid (Forward-Star Representation)
Rather than allocating dynamic linked list nodes (which fragment the heap and cause pointer-chasing cache misses), `SpatialGrid` uses a static flat index layout:
- `int *head`: Array of size $\text{num\_cells} = \text{cols} \times \text{rows}$. Stores the index of the first agent in each cell (or $-1$ if empty).
- `int *next`: Array of size $N$ (`num_agents`). Stores the index of the next agent sharing the same cell.

**Grid Construction ($\mathcal{O}(N)$):**
```c
memset(grid->head, 0xFF, num_cells * sizeof(int));
for (int i = 0; i < num_agents; i++) {
    int cell_id = compute_cell(agents[i].x, agents[i].y);
    grid->next[i] = grid->head[cell_id];
    grid->head[cell_id] = i;
}
```
Zero heap allocations occur during simulation time steps.

---

## 4. Inter-Process Communication (IPC) Protocol

The Python backend communicates with the compiled C binary via standard streams using structured JSON:
- **Input:** Parameter flags passed via `argv` (`--agents 5000 --steps 50 --threads 4 --json --compare`).
- **Output:** The native binary emits a single valid JSON string to `stdout`:
  ```json
  {
    "agents": 5000,
    "steps": 50,
    "threads": 4,
    "mode": "compare",
    "neighbors": "grid",
    "scenario": "pillars",
    "seq_time_ms": 196.945,
    "par_time_ms": 93.183,
    "speedup": 2.1135,
    "efficiency": 52.84,
    "candidate_checks": 14331506,
    "actual_neighbors": 5010470,
    "match": true,
    "bounds": true,
    "grid_cols": 80,
    "grid_rows": 30,
    "density": [[...]],
    "sample_agents": [{ "id": 0, "x": 12.4, "y": 28.1, "vx": 1.1, "vy": 0.2, "gx": 95.0, "gy": 30.0 }, ...],
    "obstacles": [{ "type": "circle", "cx": 40.0, "cy": 20.0, "radius": 4.0 }, ...]
  }
  ```
- **Error Handling:** Non-zero exit codes emit diagnostic error messages on `stderr` which the server catches and returns as HTTP 500.
