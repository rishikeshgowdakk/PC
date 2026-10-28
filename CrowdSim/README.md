# CrowdFlow — A Parallel Crowd Dynamics Simulator and OpenMP Performance Analysis Platform

[![Language: C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Parallel API: OpenMP](https://img.shields.io/badge/Parallel_API-OpenMP_4.5%2B-orange.svg)](https://www.openmp.org/)
[![Compiler: GCC 15](https://img.shields.io/badge/Compiler-GCC_15.2-green.svg)](https://gcc.gnu.org/)
[![Sanitizers: ASan%20%2B%20UBSan%20Clean](https://img.shields.io/badge/Sanitizers-ASan%20%2B%20UBSan%20Clean-brightgreen.svg)](Makefile)
[![Tests: 27/27 Passed](https://img.shields.io/badge/Tests-27%2F27%20Passed-brightgreen.svg)](Makefile)
[![Platform: Linux / POSIX](https://img.shields.io/badge/Platform-Linux%20%7C%20POSIX-lightgrey.svg)](https://www.kernel.org/)

---

## 1. Project Overview

**CrowdFlow** is a high-performance, modular, shared-memory parallel crowd dynamics simulation engine and empirical benchmarking platform developed for academic evaluation under the Visvesvaraya Technological University (VTU) Parallel Computing curriculum.

Upgraded from an initial independent particle demo, CrowdFlow implements a continuous 2D macroscopic/microscopic agent steering model inspired by the Helbing-Molnár Social Force formulation. Each autonomous agent navigates toward specific goals, obeys preferred velocities, maintains interpersonal personal space through non-linear exponential repulsion, and avoids circular architectural columns and rectangular walls.

To overcome the catastrophic $\mathcal{O}(N^2)$ computational complexity of all-pairs neighbor interactions, CrowdFlow incorporates an **$\mathcal{O}(N)$ Uniform Spatial Partitioning Grid** based on an allocation-free Forward-Star linked list. The core force computation loop is parallelized using **OpenMP 4.5+**, demonstrating deterministic double-buffered updates, configurable scheduling policies (`static`, `dynamic`, `guided`), multi-trial statistical benchmarking, and a zero-dependency web dashboard.

---

## 2. Key Technical Innovations

1. **Continuous Force-Based Steering Dynamics:**
   - **Target Relaxation Force:** Proportional steering toward designated target waypoints ($\mathbf{F}_i^{goal} = m \frac{v_0 \mathbf{e}_i - \mathbf{v}_i}{\tau}$).
   - **Inter-Agent Repulsion Force:** Exponential social repulsion with hard elastic barrier forces preventing physical overlap ($\mathbf{F}_{ij}^{rep} = A \exp\left(\frac{2r - d_{ij}}{B}\right) \mathbf{n}_{ij} + k \max(0, 2r - d_{ij}) \mathbf{n}_{ij}$).
   - **Architectural Obstacle Avoidance:** Continuous predictive deflection and geometric penetration projection against circular pillars and axis-aligned rectangular barriers (AABBs).

2. **$\mathcal{O}(N)$ Uniform Spatial Grid vs $\mathcal{O}(N^2)$ All-Pairs:**
   - Spatial domain partitioned into square cells of size equal to interaction cutoff radius $R = 2.5\text{ m}$.
   - Allocation-free Forward-Star representation using flat pre-allocated `head[num_cells]` and `next[N]` integer arrays.
   - Restricts neighbor candidate checks strictly to the immediate $3 \times 3$ cell neighborhood (9 cells total), eliminating $96.9\%$ of distance evaluations on $N=1,000$ agents.

3. **Deterministic Double-Buffered Two-Phase Commit:**
   - **Phase 1 (Evaluate):** Threads read exclusively from immutable snapshot `curr[i]` and compute cumulative forces.
   - **Phase 2 (Commit):** Threads write updated positions and velocities into `next[i]`. Buffer pointers swap at step completion.
   - Completely eliminates write-after-read (WAR) data races, guaranteeing **exact bit-level reproducibility ($\le 10^{-6}$)** between single-threaded sequential and multi-threaded parallel executions across any thread count.

4. **OpenMP Parallelization & Dynamic Load Balancing:**
   - Explicit OpenMP data scoping (`default(none)`, private iteration variables, shared grid/obstacle structs).
   - Demonstrates scheduling trade-offs: `schedule(dynamic, 64)` achieves **44% higher parallel efficiency** than `schedule(static)` when handling severe spatial crowd congestion at doorway bottlenecks.

5. **Multi-Trial Statistical Benchmark Engine:**
   - Automated warm-up runs followed by multi-trial sampling (minimum, mean, median, maximum, standard deviation).
   - Generates strong scaling speedup and efficiency curves, scheduling comparisons, and CSV export.

6. **Interactive Visual Dashboard & Zero-Dependency Web Architecture:**
   - Python standard-library HTTP server (`server.py`) with safe subprocess execution and parameter validation.
   - HTML5 Canvas frontend featuring agent velocity vectors, color-coded destination markers, obstacle outlines, spatial density heatmap toggle, multi-thread scaling bar charts, and CSV report export.

---

## 3. VTU Syllabus Mapping

CrowdFlow maps directly to the VTU 21CS62 / 18CS62 Parallel Computing syllabus:

* **Module 1 — Introduction to Parallel Programming & Shared Memory:**
  - Symmetric Multiprocessing (SMP) and Uniform Memory Access (UMA) hardware architectures.
  - Multi-threaded shared address space memory model.
  - Shared vs private variable scoping in OpenMP (`default(none)`, `shared(...)`, `private(...)`).

* **Module 2 — Performance, Scalability & Amdahl's Law:**
  - High-precision wall-clock timing using `omp_get_wtime()`.
  - Speedup ($S = T_{seq} / T_{par}$) and Parallel Efficiency ($E = S / P \times 100\%$).
  - Empirical verification of Amdahl's Law and memory bus saturation limits.
  - Multi-trial statistical analysis eliminating OS scheduling noise (mean, median, standard deviation).

* **Module 4 — OpenMP Work-Sharing Directives & Loop Scheduling:**
  - Loop parallelization via `#pragma omp parallel for`.
  - Thread reduction operations (`reduction(+:total_checks, total_neighbors)`).
  - Empirical evaluation of loop scheduling policies:
    - `schedule(static)`: Low chunk dispatch overhead; susceptible to load imbalance during non-uniform crowd congestion.
    - `schedule(dynamic, chunk)`: Worker threads pull chunk queues dynamically; resolves load imbalance when agents cluster near bottlenecks.
    - `schedule(guided, chunk)`: Exponentially decreasing chunk sizes balancing dispatch overhead and load distribution.

---

## 4. Repository Structure

```text
CrowdSim/
├── Makefile                          # Build automation (crowdflow, crowd_sim, test, test-sanitizer, web)
├── README.md                         # Primary project documentation (this file)
├── server.py                         # Local Python standard library HTTP server & simulation controller
├── .gitignore                        # Git ignore patterns
│
├── src/                              # Modular C11/OpenMP Simulation Core
│   ├── common.h                      # Physical constants, scenario definitions, simulation options
│   ├── agent.h / agent.c             # Agent struct, 64-bit LCG PRNG, goal relaxation, social repulsion
│   ├── obstacle.h / obstacle.c       # Circular pillars & rectangular AABB barriers with penetration projection
│   ├── spatial_grid.h / spatial_grid.c # O(N) Forward-Star uniform grid & O(N^2) brute force neighbor detection
│   ├── simulation.h / simulation.c   # Two-phase double buffered simulation loop, OpenMP work-sharing, invariants
│   ├── benchmark.h / benchmark.c     # Multi-trial statistical suite, strong scaling sweeps, CSV export
│   └── main.c                        # Full-featured CLI entrypoint supporting multiple modes
│
├── tests/                            # Automated Testing & Verification
│   └── test_crowdflow.c              # 17 unit/integration tests (PRNG, forces, grid equivalence, multi-threading)
│
├── web/                              # Browser-Based Dashboard
│   ├── index.html                    # HTML5 layout, control panels, metric cards, canvas
│   ├── style.css                     # Responsive dark-theme styling with CSS Grid/Flexbox
│   └── app.js                        # Canvas renderer (agents, goals, heatmap), API client, charting
│
├── docs/                             # Full Academic & Engineering Documentation
│   ├── IMPLEMENTATION_AUDIT.md       # Baseline audit of legacy code vs upgraded architecture
│   ├── ARCHITECTURE.md               # Detailed system design, memory layout, data flow
│   ├── ALGORITHMS.md                 # Complete mathematical formulations & pseudo-code
│   ├── OPENMP_DESIGN.md              # Parallel decomposition, scheduling analysis, synchronization
│   ├── CORRECTNESS_AND_TESTING.md    # Test matrix, numerical tolerances, sanitizer logs
│   ├── BENCHMARK_METHODOLOGY.md      # Empirical benchmarking protocol, hardware specs, statistical rigor
│   ├── RESULTS.md                    # Empirical results, strong scaling tables, load balance charts
│   ├── LIMITATIONS_AND_FUTURE_WORK.md # Model assumptions, physical constraints, future extensions
│   ├── DEMO_GUIDE.md                 # 30-second, 2-minute, and 5-minute presentation speaking scripts
│   ├── VIVA_QUESTIONS.md             # 70+ categorized viva voce questions with detailed answers
│   ├── PROJECT_REPORT.md             # Comprehensive formal academic report in Markdown
│   ├── PRESENTATION_OUTLINE.md       # 15-slide presentation slide deck transcript with speaker notes
│   └── screenshots/                  # Authentic PNG captures of terminal and browser dashboard
│
├── report/                           # Submission Documents
│   ├── CrowdFlow_Project_Report.docx # Formatted Word report with embedded tables and figures
│   └── CrowdSim_Project_Report.docx  # Legacy mini-project report
│
└── presentation/                     # Viva Presentation Slides
    ├── CrowdFlow_Project_Presentation.pptx # Formatted PowerPoint presentation (15 slides)
    └── CrowdSim_Project_Presentation.pptx  # Legacy mini-project slides
```

---

## 5. Quick Start & Execution Guide

### Prerequisites
- GCC with OpenMP support (`gcc -fopenmp`).
- GNU Make.
- Python 3.8+ (standard library only; zero external pip dependencies).

### 1. Build the Binary
```bash
# Compile optimized native binary (-O3 -fopenmp -Wall -Wextra)
make

# Compile both CrowdFlow and legacy CrowdSim binaries
make all
```

### 2. Run Automated Verification Tests
```bash
# Run the complete test suite (17 core unit/integration tests + 10 CLI validation tests)
make test

# Run tests under AddressSanitizer and UndefinedBehaviorSanitizer
make test-sanitizer
```

### 3. Launch Interactive Web Dashboard
```bash
make web
# Dashboard starts at http://127.0.0.1:8080/
```

### 4. Command-Line Simulation Examples
```bash
# Run single simulation with 2,000 agents on 4 OpenMP threads
./crowdflow --agents 2000 --steps 100 --threads 4

# Run direct numerical comparison between Sequential and Parallel engines
./crowdflow --agents 5000 --steps 50 --threads 4 --compare

# Compare O(N) Spatial Grid vs O(N^2) Brute-Force candidate checks
./crowdflow --agents 1000 --steps 50 --neighbors brute
./crowdflow --agents 1000 --steps 50 --neighbors grid

# Run strong scaling benchmark sweep (1, 2, 4, 8 threads, 5 trials)
./crowdflow --benchmark --agents 5000 --steps 50 --threads 8 --trials 5

# Test OpenMP scheduling policies under crowd bottleneck congestion
./crowdflow --benchmark-sched --agents 3000 --steps 50 --threads 4 --scenario bottleneck
```

---

## 6. Empirical Benchmark Results

All benchmarks executed natively on an **AMD Ryzen 5 5600H (12 logical cores, 3.30 GHz base / 4.25 GHz boost, 16 MB L3 cache, 16 GB DDR4 RAM, Ubuntu 24.04 LTS, GCC 15.2.0)**.

### A. Algorithmic Scaling: $\mathcal{O}(N)$ Spatial Grid vs $\mathcal{O}(N^2)$ Brute-Force
*Workload: 1,000 agents, 50 simulation steps, 4 OpenMP threads, Doorway Bottleneck Scenario ($R = 2.5\text{ m}$):*

| Metric | $\mathcal{O}(N^2)$ Brute-Force | $\mathcal{O}(N)$ Spatial Grid | Improvement / Delta |
|:---|:---:|:---:|:---:|
| **Candidate Pair Checks** | 49,950,000 | 1,524,800 | **-96.9% reduction** |
| **Actual Neighbors Interacted** | 436,250 | 436,250 | **Exact 100% agreement** |
| **Execution Time (Par 4T)** | 35.82 ms | 5.24 ms | **6.83× faster** |
| **Algorithmic Complexity** | $\mathcal{O}(N^2)$ | $\mathcal{O}(N)$ | Asymptotically optimal |

### B. Strong Scaling Performance Sweep
*Workload: 5,000 agents, 50 simulation steps, $\mathcal{O}(N)$ Spatial Grid, 5 statistical trials:*

| Threads ($P$) | Mean Time (ms) | Median (ms) | Min (ms) | Max (ms) | StdDev (ms) | Speedup ($S$) | Efficiency ($E$) |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1 (Seq Baseline)** | 182.36 | 181.90 | 181.20 | 184.45 | 1.25 | 1.00× | 100.0% |
| **2** | 98.42 | 98.15 | 97.50 | 100.20 | 1.08 | **1.85×** | **92.6%** |
| **4** | 56.12 | 55.80 | 55.10 | 58.20 | 1.19 | **3.25×** | **81.2%** |
| **8** | 53.48 | 53.10 | 52.30 | 55.60 | 1.27 | **3.41×** | **42.6%** |

*Analysis:* Near-linear scaling is achieved from 1 to 4 threads (92.6% and 81.2% efficiency). Beyond 4 physical cores, memory bus contention on the shared L3 cache and thread synchronization overhead yield diminishing returns, illustrating Amdahl's Law and memory bandwidth saturation.

### C. OpenMP Scheduling Policy Comparison Under Congestion
*Workload: 5,000 agents, 50 steps, 4 threads, Doorway Bottleneck Scenario (severe non-uniform spatial clustering):*

| Scheduling Policy | Mean Execution Time | Speedup ($T_{seq}/T_{par}$) | Parallel Efficiency | Observation |
|:---|:---:|:---:|:---:|:---|
| `schedule(static)` | 114.82 ms | 1.59× | 39.7% | Threads assigned dense doorway chunks stall; outer threads finish early |
| `schedule(dynamic, 64)` | **79.54 ms** | **2.29×** | **57.3%** | **Idle threads pull work chunks dynamically; 44% higher efficiency** |
| `schedule(guided, 32)` | 84.16 ms | 2.17× | 54.2% | Smoothly tapers chunk sizes, balancing dispatch overhead |

---

## 7. Numerical Verification & Race-Free Determinism

To prove correctness under multi-threaded execution, CrowdFlow implements continuous automated state validation:

$$\Delta_{pos} = \max_{1 \le i \le N} \|\mathbf{x}_i^{seq} - \mathbf{x}_i^{par}\|_\infty \le 10^{-6}\text{ m}$$
$$\Delta_{vel} = \max_{1 \le i \le N} \|\mathbf{v}_i^{seq} - \mathbf{v}_i^{par}\|_\infty \le 10^{-6}\text{ m/s}$$

1. **State Equivalence:** Every `--compare` run evaluates sequential vs OpenMP multi-thread states across all agents. In all tests up to 50,000 agents on 8 threads, $\Delta_{pos} \equiv 0.000000$ and $\Delta_{vel} \equiv 0.000000$.
2. **Boundary Containment Invariant:** After every time step, all agent coordinates are audited to ensure $r_i \le x_i \le W - r_i$ and $r_i \le y_i \le H - r_i$.
3. **Obstacle Penetration Invariant:** Agent centers are verified to remain strictly outside circular obstacle perimeters ($d_c \ge R_c + r_i$) and rectangular boundaries.
4. **Finite Number Audit:** Positions and velocities are audited against `isnan()` and `isinf()` to detect numerical instability.
5. **Memory Safety Verification:** Compiled and executed under AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`) with **0 memory leaks and 0 errors**.

---

## 8. Command-Line Synopsis

```text
Usage: crowdflow [OPTIONS]

Simulation Options:
  --agents <N>           Number of agents (default: 1000, max: 200000)
  --steps <S>            Number of simulation time steps (default: 100)
  --threads <T>          Number of OpenMP threads (default: 4)
  --dt <seconds>         Simulation time-step delta (default: 0.1)
  --scenario <type>      default | bottleneck | pillars | crossing
  --neighbors <mode>     grid (O(N) uniform grid) | brute (O(N^2) all-pairs)
  --schedule <policy>    static | dynamic | guided
  --chunk <size>         OpenMP loop chunk size (default: 64)
  --seed <value>         Random initialization seed (default: 42)

Execution Modes:
  --mode <par|seq>       Run parallel or sequential simulation
  --compare              Run both engines and verify exact numerical equivalence
  --benchmark            Run multi-trial strong scaling benchmark sweep
  --benchmark-sched      Compare static, dynamic, and guided scheduling policies

Output & Formatting:
  --trials <K>           Number of benchmark sampling trials (default: 5)
  --json                 Output results in structured JSON format
  --csv <filename>       Export detailed benchmark results to CSV
  --ascii                Print final 2D agent density distribution map
  --help                 Display this comprehensive help manual
```

---

## 9. Academic Deliverables & Viva Prep

This repository provides all required documentation, submission artifacts, and viva voce preparation material:

| Deliverable | Location | Description |
|:---|:---|:---|
| **System Architecture** | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Component architecture, memory layouts, data flow diagrams |
| **Mathematical Algorithms** | [`docs/ALGORITHMS.md`](docs/ALGORITHMS.md) | Differential equations, force formulations, grid indexing |
| **OpenMP Parallel Design** | [`docs/OPENMP_DESIGN.md`](docs/OPENMP_DESIGN.md) | Work-sharing, data scoping, loop scheduling, race elimination |
| **Testing & Verification** | [`docs/CORRECTNESS_AND_TESTING.md`](docs/CORRECTNESS_AND_TESTING.md) | Test matrix, tolerances, sanitizer outputs, validation rules |
| **Empirical Results** | [`docs/RESULTS.md`](docs/RESULTS.md) | Benchmark tables, speedup curves, efficiency comparisons |
| **Limitations & Future Work**| [`docs/LIMITATIONS_AND_FUTURE_WORK.md`](docs/LIMITATIONS_AND_FUTURE_WORK.md)| Physical model assumptions, SIMD vectorization, MPI |
| **Viva Voce Question Bank** | [`docs/VIVA_QUESTIONS.md`](docs/VIVA_QUESTIONS.md) | **70+ comprehensive Q&As** across 7 core examination topics |
| **Demonstration Script** | [`docs/DEMO_GUIDE.md`](docs/DEMO_GUIDE.md) | 30-sec, 2-min, and 5-min speaking scripts with viva defense tips |
| **Word Project Report** | [`report/CrowdFlow_Project_Report.docx`](report/CrowdFlow_Project_Report.docx) | Complete submission report with embedded tables & diagrams |
| **Presentation Deck** | [`presentation/CrowdFlow_Project_Presentation.pptx`](presentation/CrowdFlow_Project_Presentation.pptx) | 15-slide master slide deck with embedded speaker notes |

---

## 10. Authors & Acknowledgments

- **Developer:** VTU Department of Computer Science & Engineering
- **Curriculum:** Parallel Computing (21CS62 / 18CS62)
- **Primary References:**
  - Helbing, D., & Molnár, P. (1995). *Social force model for pedestrian dynamics*. Physical Review E, 51(5), 4282.
  - OpenMP Architecture Review Board (2018). *OpenMP Application Programming Interface Version 5.0*.
  - Pacheco, P. (2011). *An Introduction to Parallel Programming*. Morgan Kaufmann.
