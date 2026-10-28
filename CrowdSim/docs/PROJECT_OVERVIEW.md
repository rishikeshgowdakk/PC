# CrowdSim — Project Overview

## 1. Executive Summary

**CrowdSim** (*Sequential and OpenMP-Based Parallel Particle Movement Simulator*) is an educational parallel computing application designed for the Visvesvaraya Technological University (VTU) Parallel Computing curriculum.

The project models the movement of multiple independent point particles inside a bounded two-dimensional rectangular environment (100.0 × 40.0 units). Each particle possesses coordinates `(x, y)` and velocities `(vx, vy)`. Over a sequence of discrete simulation time steps, positions are updated and reflected whenever boundaries are reached.

CrowdSim executes this simulation through two distinct engines:
1. **Sequential Engine:** An ordinary single-threaded C loop serving as the reference baseline.
2. **Parallel Engine:** A multi-threaded implementation utilizing **OpenMP** shared-memory parallelism (`#pragma omp parallel for schedule(static)`).

Both engines start from the exact same deterministic initial state. Upon completion, the program:
- Measures high-resolution execution time using `omp_get_wtime()`.
- Calculates **Speedup** ($S = T_{seq} / T_{par}$) and **Parallel Efficiency** ($E = S / P \times 100\%$).
- Performs an element-by-element floating-point verification confirming that parallel execution preserved exact physical correctness.
- Verifies that all particles remained within defined environment bounds.
- Renders an ASCII or Canvas density distribution map.

---

## 2. Problem Statement & Motivation

Simulating dynamic systems involving thousands or millions of autonomous entities (particles, agents, molecules) requires updating state vectors repeatedly over many time steps. In single-threaded execution, the workload scales linearly as $\mathcal{O}(N \times S)$, where $N$ is the particle count and $S$ is the step count.

Because each particle's movement and wall reflection depends strictly on its own state during a given time step, the computational loop exhibits **embarrassing parallelism** (data independence). CrowdSim investigates how effectively shared-memory multi-core hardware can accelerate this workload using OpenMP directives, while exploring real-world parallel computing trade-offs such as thread creation overhead, memory bandwidth, and Amdahl's Law.

---

## 3. System Architecture & Components

CrowdSim adopts a modular 3-tier architecture:

```
┌────────────────────────────────────────────────────────┐
│               Web Browser (User Interface)             │
│   • HTML5 Canvas Visualisation (Heatmap)               │
│   • Responsive Parameter Controls & Status Display     │
│   • Performance Metrics & Execution Logging            │
└───────────────────────────▲────────────────────────────┘
                            │ HTTP REST API (JSON)
┌───────────────────────────▼────────────────────────────┐
│          Local Backend Server (server.py)              │
│   • Python 3 Standard Library HTTP Server              │
│   • Request Validation & Range Sanitisation            │
│   • Process Lock & Safe Subprocess Invocation          │
└───────────────────────────▲────────────────────────────┘
                            │ Subprocess IPC / CLI (--json)
┌───────────────────────────▼────────────────────────────┐
│      Native Simulation Engine (crowd_sim / C11)        │
│   • Deterministic Initialisation                       │
│   • Sequential & OpenMP Simulation Loops               │
│   • Mathematical Wall Folding (fmod Reflection)        │
│   • Strict Validation (Tolerance 1e-9)                 │
└────────────────────────────────────────────────────────┘
```

### Component Roles

1. **`crowd_sim.c` (Core Native Engine):**
   - Implemented in ISO C11 with GCC and OpenMP.
   - Zero external libraries beyond standard libc and `-lm`.
   - Supports dual output modes: formatted human-readable terminal text and machine-readable JSON (`--json`).

2. **`server.py` (Integration Controller):**
   - Pure Python standard library (`http.server`, `subprocess`, `json`).
   - Runs exclusively on `localhost` (`127.0.0.1:8080`).
   - Protects against thread concurrency collisions via `threading.Lock`.

3. **`web/` (Browser Dashboard):**
   - `index.html`: Clean, accessible semantic HTML structure.
   - `style.css`: Modern dark-theme aesthetic, flexible CSS Grid layout, fully responsive on mobile/desktop.
   - `app.js`: Asynchronous fetch client, canvas heatmap renderer, client-side input validation.

---

## 4. VTU Syllabus Mapping

CrowdSim directly fulfills key laboratory learning outcomes in the VTU Parallel Computing curriculum:

* **Module 1 — Introduction to Parallel Programming & Shared Memory:**
  - Explores shared-memory hardware architectures (MIMD multi-core processors).
  - Demonstrates coordinating worker threads that share a common address space.
  - Implements thread isolation where each worker manipulates private loop indices and disjoint array elements.

* **Module 2 — Performance, Timings & Scalability:**
  - Uses `omp_get_wtime()` for high-resolution wall-clock benchmarking.
  - Formulates and calculates Speedup: $S = T_{seq} / T_{par}$.
  - Formulates and calculates Parallel Efficiency: $E = (S / P) \times 100\%$.
  - Analyzes why small workloads ($N < 1000$) experience speedup $< 1.0\times$ due to thread fork-join overhead.

* **Module 4 — OpenMP Directives & Thread Safety:**
  - Practical application of `#pragma omp parallel for schedule(static)`.
  - Understanding variable scoping: loop index `i` is implicitly private; particle pointer `p` is shared.
  - Verification of data-race-free computation without requiring locks or critical sections.

---

## 5. Build, Run, and Test Instructions

### Prerequisites
- GCC supporting OpenMP (`gcc -fopenmp`).
- GNU Make.
- Python 3.8+ (standard library only).

### Quick Commands

```bash
# 1. Compile the native simulation binary
make

# 2. Run automated verification test suite (15 test cases)
make test

# 3. Launch the browser dashboard
make web
# Then open http://127.0.0.1:8080 in your browser

# 4. Run directly from terminal with custom arguments
./crowd_sim --particles 100000 --steps 500 --threads 4

# 5. Clean generated binaries
make clean
```

---

## 6. Understanding the Outputs

### Terminal Output
When run from the command line, CrowdSim prints:
- Simulation configuration (particle count, steps, threads).
- Measured sequential execution time in milliseconds.
- Measured parallel execution time in milliseconds.
- Computed speedup factor (e.g. `2.22x`).
- Computed parallel efficiency percentage (e.g. `55.5%`).
- Correctness result: `MATCH` (all coordinates within $10^{-9}$ tolerance) or `MISMATCH`.
- Boundary audit: `PASS` (all particles within $[0, 100] \times [0, 40]$) or `FAIL`.
- An ASCII density grid showing where particles settled.

### Web Dashboard Output
When run via the browser:
- An interactive Canvas visualises particle density as a coloured heatmap (navy blue for low density, cyan for medium, bright yellow for high clusters).
- The metrics table displays precise millisecond timings, speedup, and efficiency.
- Green status badges confirm `MATCH` and `PASS`.
- An execution log records timestamped milestones.
