# CrowdFlow Implementation Audit & Baseline Analysis

**Date:** 2026-10-29  
**Reviewer:** Senior Systems & Parallel Computing Engineer  
**Project Transition:** CrowdSim (Baseline) $\longrightarrow$ CrowdFlow (Target System)

---

## 1. Executive Summary & Audit Scope

This document provides a rigorous technical audit of the existing **CrowdSim** repository prior to executing the comprehensive upgrade into **CrowdFlow** (*A Parallel Crowd Dynamics Simulator and OpenMP Performance Analysis Platform*).

The audit assesses the repository structure, C source code, OpenMP implementation, algorithmic complexity, numerical stability, testing framework, build pipeline, and user interface. It establishes reproducible baseline performance benchmarks and specifies the architectural roadmap and acceptance criteria for all subsequent engineering phases.

---

## 2. Existing Repository Architecture

### 2.1 File Inventory
The pre-upgrade repository consists of:
- `CrowdSim/crowd_sim.c`: Monolithic C11 source file (~380 lines) containing all simulation logic, CLI argument parsing, OpenMP loops, ASCII grid generation, and JSON formatting.
- `CrowdSim/Makefile`: Build automation file supporting `all`, `run`, `test`, `web`, and `clean` targets.
- `CrowdSim/server.py`: Python 3 standard-library HTTP server handling API requests (`POST /api/simulate`) via subprocess invocation.
- `CrowdSim/web/`: Minimal web dashboard (`index.html`, `style.css`, `app.js`).
- `CrowdSim/docs/`: Preliminary documentation, viva preparation, and test reports.
- `CrowdSim/.gitignore`: Build artifact exclusion rules.

### 2.2 Git Status & Working Tree
- Active Branch: `main`
- Untracked Directories: `docs/`, `presentation/`, `report/`, `web/`
- Working Directory: Cleanly isolatable under `CrowdSim/`

---

## 3. Baseline Feature Analysis: Implemented vs. Proposed

| Feature Area | Baseline Status (CrowdSim) | Deficiencies / Technical Gap | Target Status (CrowdFlow) |
|---|---|---|---|
| **Agent Representation** | Simple point particle: `(x, y, vx, vy)` | No goals/destinations, no preferred speeds, no radii, no IDs | Full Agent: ID, `(x,y)`, `(vx,vy)`, `(gx,gy)`, $v_{\text{pref}}$, $r_{\text{interact}}$, mass |
| **Motion Model** | Ballistic inertial movement: $\mathbf{r} \leftarrow \mathbf{r} + \mathbf{v}$ | Zero intent, zero target steering, constant velocity | Goal-directed steering force + separation dynamics |
| **Obstacles** | None (empty rectangular arena) | Cannot simulate bottlenecks, pillars, or room layouts | Static obstacles (circles & axis-aligned rectangles) |
| **Inter-Agent Interaction**| **None** (zero interaction, complete pass-through) | Not crowd dynamics; physically trivial embarrassingly parallel | Local collision avoidance / social separation forces |
| **Neighbor Detection** | Not applicable (no interactions) | Naive pairwise check is $\mathcal{O}(N^2)$ | **Uniform Spatial Grid** ($\mathcal{O}(N)$) vs. Brute-Force reference |
| **Two-Phase Update** | In-place update | Unsafe for interacting agents (race conditions on reads) | Double-buffered two-phase commit: read snapshot $\to$ commit |
| **Parallel Scheduling** | `schedule(static)` only | Cannot compare scheduling policies under imbalanced workloads | Configurable `static`, `dynamic`, and `guided` scheduling |
| **Benchmarking System** | Single-run wall-clock timing | High jitter vulnerability; lacks statistical repetitions | Multi-trial benchmarking with mean, median, stddev, and CSV export |
| **Visualization** | Static end-state density grid | Does not show individual agents, obstacles, or goals | Interactive 2D simulation canvas showing agents, targets, and obstacles |

---

## 4. Baseline Build & Test Results

The baseline build and automated test suite were executed on the test host:
- **Operating System:** Linux (Ubuntu 6.17 kernel, x86_64)
- **Compiler:** GCC 15.2.0 (`gcc -std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic -lm`)
- **CPU:** 12-core x86_64 Symmetric Multiprocessing (SMP)

### 4.1 Build Result
```bash
gcc -std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic -o crowd_sim crowd_sim.c -fopenmp -lm
```
*Result:* Built with 0 errors, 0 compiler warnings.

### 4.2 Automated Test Suite (`make test`)
- 15 out of 15 automated test cases **PASSED**.
- Test execution time: ~1.8 seconds.
- Validated: 1, 4, 8 thread equivalence for independent particles; boundary reflection; input argument rejection.

### 4.3 Baseline Benchmark Performance (Independent Particles)
Configuration: 50,000 particles, 200 simulation steps, 4 OpenMP threads:
- **Sequential Runtime ($T_{seq}$):** $30.503\text{ ms}$
- **Parallel Runtime ($T_{par}$):** $14.466\text{ ms}$
- **Measured Speedup ($S$):** $2.11\times$
- **Parallel Efficiency ($E$):** $52.7\%$
- **Verification Status:** `MATCH` (tolerance $\le 10^{-9}$), `PASS` (100% within bounds).

---

## 5. Algorithmic Complexity & Parallelization Analysis

### 5.1 Baseline Algorithmic Complexity
In CrowdSim, each particle $i \in [0, N-1]$ updates independently:
$$x_i \leftarrow x_i + vx_i, \quad y_i \leftarrow y_i + vy_i$$
- **Time Complexity per Step:** $\mathcal{O}(N)$
- **Total Time Complexity ($S$ steps):** $\mathcal{O}(N \times S)$
- **Inter-particle dependency:** **Zero** ($\mathcal{O}(1)$ work per particle).

### 5.2 Why CrowdSim is Not a True Parallel Computing Challenge
Because CrowdSim particles have zero interactions, the loop is trivially embarrassingly parallel. There is no cache contention between particles, no spatial queries, and no dynamic load imbalance.

In real crowd simulations:
1. Agents must query nearby neighbours within an interaction radius $r_{\text{interact}}$.
2. Naive pairwise comparison requires $\frac{N(N-1)}{2} = \mathcal{O}(N^2)$ distance checks per step. At $N = 50,000$, this requires $1.25 \times 10^9$ distance checks per time step—completely intractable sequentially.
3. Spatial partitioning (Uniform Grid / Spatial Hash) reduces average neighbor search complexity to $\mathcal{O}(N)$.
4. Constructing and querying a spatial grid concurrently introduces real multi-threaded engineering challenges: race-free binning, thread scheduling under non-uniform agent densities, and cache line sharing.

---

## 6. Known Limitations & Deficiencies in Baseline

1. **Non-Realistic Physics:** Particles bounce indefinitely like billiard balls without friction, heading, or goals.
2. **Absence of Spatial Partitioning:** No data structures exist for spatial queries.
3. **Double Buffering Missing:** Updating agent $i$'s position in-place while another thread reads $i$'s position to compute agent $j$'s avoidance force would cause non-deterministic read-after-write (RAW) race conditions.
4. **Monolithic Source Code:** Single 380-line C file lacks modular abstraction, making it unmaintainable as algorithmic complexity increases.
5. **No Statistical Benchmarking:** Single-run timing cannot compute standard deviation or filter OS scheduling outliers.

---

## 7. Proposed Upgrades & Architectural Roadmap

### Phase B: Crowd Dynamics Simulation Engine
- Evolve particle representation into `Agent` struct: stable ID, position, velocity, goal coordinate, preferred speed, interaction radius, avoidance force.
- Implement goal-seeking steering force:
  $$\mathbf{F}_{\text{goal}} = \frac{\mathbf{v}_{\text{pref}} - \mathbf{v}}{\tau}$$
- Implement smooth agent-agent repulsive separation force:
  $$\mathbf{F}_{\text{repulse}} = \sum_{j \in \text{neighbors}} A \exp\left(\frac{2r - d_{ij}}{B}\right) \hat{\mathbf{n}}_{ij}$$
- Implement static obstacles (circular pillars and rectangular barriers) with non-penetration impulse response.
- Enforce **two-phase double buffering**: `current_agents` $\to$ `next_agents` committed atomically at the end of each step.

### Phase C: Spatial Partitioning Grid ($\mathcal{O}(N)$ vs $\mathcal{O}(N^2)$)
- Implement a 2D uniform spatial grid dividing the $W \times H$ world into cells of size $R_{\text{cell}} \ge r_{\text{interact}}$.
- Support brute-force neighbor search as a verifiable $\mathcal{O}(N^2)$ baseline.
- Implement candidate check counters to empirically prove work reduction.
- Validate grid neighbor queries against brute-force queries on identical agent configurations.

### Phase D: Modular OpenMP Architecture
- Refactor codebase into clean modules:
  - `src/agent.h`, `src/agent.c`: Agent physics and steering models.
  - `src/obstacle.h`, `src/obstacle.c`: Geometric obstacles and collision handling.
  - `src/spatial_grid.h`, `src/spatial_grid.c`: Uniform spatial hash grid.
  - `src/simulation.h`, `src/simulation.c`: Orchestrator (sequential and OpenMP paths).
  - `src/benchmark.h`, `src/benchmark.c`: High-resolution multi-run benchmarking engine.
  - `src/main.c`: Command-line interface with CSV and JSON output modes.
- Support OpenMP loop scheduling: `schedule(static)`, `schedule(dynamic, chunk)`, `schedule(guided)`.
- Use explicit OpenMP data scoping (`default(none)` where appropriate).

### Phase E: Advanced Benchmarking & Scalability
- Support configurable agent counts ($1\text{k}, 5\text{k}, 10\text{k}, 25\text{k}, 50\text{k}$), steps, threads ($1, 2, 4, 8, 12$), seeds, and trials.
- Output machine-readable CSV for automated graphing.
- Formulate and report:
  - Algorithmic Speedup: $T_{\text{brute}} / T_{\text{grid}}$
  - Parallel Speedup: $T_{\text{grid, seq}} / T_{\text{grid, par}}$
  - Parallel Efficiency: $(S / P) \times 100\%$

### Phase F: Web Dashboard Upgrade
- Modernize browser dashboard to display real agent coordinates, goal vectors, and obstacles on HTML5 Canvas.
- Add controls for neighbor algorithm (Brute-Force vs. Grid), scheduling policy, agent count, thread count.
- Embed interactive charts for thread scaling and algorithm comparison.

---

## 8. Acceptance Criteria Matrix

| Item | Requirement | Verification Method |
|---|---|---|
| **AC-01** | Agent movement models destination seeking and separation forces | State transitions verified via unit tests |
| **AC-02** | Obstacles cleanly deflect agents without penetration | Coordinate boundary tests with circular & rectangular obstacles |
| **AC-03** | Brute-force and spatial-grid yield identical neighbor forces | Comparator test on $N=500$ agents matches within $10^{-6}$ |
| **AC-04** | Spatial grid reduces candidate distance checks by $> 80\%$ on $N \ge 1000$ | Candidate check counter logged and verified |
| **AC-05** | Sequential and OpenMP parallel runs produce equivalent results | Double-buffering prevents data races; tolerance check passes |
| **AC-06** | OpenMP thread count and scheduling policies configurable via CLI | Verified via `--threads` and `--schedule` CLI flags |
| **AC-07** | AddressSanitizer and UndefinedBehaviorSanitizer clean | Compiled with `-fsanitize=address,undefined` with zero defects |
| **AC-08** | Automated test suite passes 100% | `make test` executes unit & integration tests cleanly |
| **AC-09** | Machine-readable CSV output supported | `--csv` emits structured performance tables |
| **AC-10** | Web dashboard visualizes real C simulation data | End-to-end browser execution verified via headless Chrome |
