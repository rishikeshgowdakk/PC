# CrowdFlow Presentation Outline & Slide Deck Specifications (15 Slides)

---

## Slide 1: Title Slide
* **Title:** CrowdFlow: A Parallel Crowd Dynamics Simulator & OpenMP Performance Analysis Platform
* **Subtitle:** VTU Parallel Computing Mini-Project (Modules 1, 2, & 4)
* **Author / Presenter:** [Student Name] (USN: [USN])
* **Department & Guide:** Department of Computer Science & Engineering • Guided by: [Guide Name]
* **Speaker Notes:**
  > *"Good morning, respected guide and examiners. Today I am presenting our parallel computing project, 'CrowdFlow: A Parallel Crowd Dynamics Simulator and OpenMP Performance Analysis Platform'. This project transitions beyond basic independent particles into true crowd dynamics, implementing continuous steering forces, an O(N) spatial partitioning grid, and multi-core OpenMP acceleration."*

---

## Slide 2: Problem Statement: Algorithmic & Hardware Scaling
* **The Physics Challenge:** Real crowd dynamics requires computing inter-agent avoidance forces for every agent.
* **The Algorithmic Bottleneck:** Naive pairwise distance checking requires $N(N-1)/2 = \mathcal{O}(N^2)$ calculations per time step.
  - At 50,000 agents: **1.25 billion distance calculations per step**.
* **The Hardware Bottleneck:** Single-threaded execution leaves multi-core hardware idle.
* **Engineering Objective:** Combine an $\mathcal{O}(N)$ uniform spatial grid with OpenMP multi-core parallelism to achieve high simulation throughput while guaranteeing numerical correctness.
* **Speaker Notes:**
  > *"In crowd simulations, pedestrians interact with their neighbors. Naively checking all agent pairs requires O(N^2) checks—over 1.25 billion checks per step for 50,000 agents. Our objective is twofold: first, design an O(N) spatial grid to eliminate unnecessary calculations; second, parallelize the workload across CPU cores using OpenMP."*

---

## Slide 3: Project Objectives
* **1. Continuous Crowd Dynamics:** Implement goal-directed steering, social separation forces, and obstacle avoidance.
* **2. Spatial Grid Partitioning:** Reduce neighbor candidate checks from $\mathcal{O}(N^2)$ to $\mathcal{O}(N)$ using a uniform spatial grid.
* **3. Race-Free OpenMP Architecture:** Implement a double-buffered two-phase commit design eliminating data races.
* **4. Scheduling Policy Analysis:** Benchmark `static`, `dynamic`, and `guided` scheduling under crowded bottlenecks.
* **5. Verification & Visualization:** Validate 100% equivalence between sequential and parallel runs, and visualize on an interactive web dashboard.
* **Speaker Notes:**
  > *"We set out five clear objectives: first, implement continuous crowd dynamics; second, build an efficient spatial partitioning grid; third, implement a race-free OpenMP architecture; fourth, benchmark scheduling policies under load imbalance; and fifth, build an interactive dashboard."*

---

## Slide 4: Theoretical Background: Shared-Memory Concurrency
* **MIMD Symmetric Multiprocessing:** Multiple physical processor cores sharing a common RAM address space.
* **Fork-Join Parallel Execution:** Master thread forks a team of workers upon entering `#pragma omp parallel`, partitions iterations, and synchronizes at an implicit barrier.
* **Two-Phase Double-Buffering:**
  - Phase 1 (Compute): All threads read from immutable `current_agents` and write strictly to assigned indices in `next_agents`.
  - Phase 2 (Commit): Fast pointer swap in $\mathcal{O}(1)$ time. Zero lock contention!
* **Speaker Notes:**
  > *"We utilize shared-memory symmetric multiprocessing. To prevent race conditions when multiple threads read neighbor positions while updating them, we use double-buffering. Threads read from a consistent snapshot and write to disjoint indices in a secondary buffer, ensuring thread safety without mutex locks."*

---

## Slide 5: System Architecture & Component Decomposition
* **Modular ISO C11 Engine:**
  - `src/agent.c`: Kinematics, goal relaxation, social repulsion forces.
  - `src/obstacle.c`: Circular pillars and rectangular barriers with penetration resolution.
  - `src/spatial_grid.c`: Uniform spatial hash grid (Forward-Star array representation).
  - `src/simulation.c`: Two-phase step orchestrator (Sequential & OpenMP).
  - `src/benchmark.c`: Multi-trial statistical benchmarking engine.
* **Integration Layer:** Python standard library server (`127.0.0.1:8080`) providing safe JSON IPC.
* **Presentation Layer:** Interactive HTML5 Canvas dashboard with density heatmaps.
* **Speaker Notes:**
  > *"CrowdFlow is organized into clean modules: agent kinematics, obstacles, spatial indexing, simulation orchestration, and statistical benchmarking. A lightweight Python server bridges the native C binary to our HTML5 Canvas dashboard."*

---

## Slide 6: Continuous Crowd Dynamics Formulation
* **1. Goal-Seeking Acceleration:**
  $$\mathbf{F}_{\text{goal}} = \frac{\mathbf{v}_{\text{des}} - \mathbf{v}}{\tau}, \quad \mathbf{v}_{\text{des}} = v_{\text{pref}} \cdot \frac{\mathbf{g} - \mathbf{x}}{\|\mathbf{g} - \mathbf{x}\|}$$
* **2. Inter-Agent Social Separation:**
  $$\mathbf{F}_{\text{agents}} = \sum_{j \in \mathcal{N}_i} A \exp\left(-\frac{d_{ij} - 2r}{B}\right) \hat{\mathbf{n}}_{ij}$$
* **3. Obstacle Avoidance & Penetration Resolution:**
  Repulsive outward force within buffer margin; geometric projection if agent penetrates physical boundary.
* **Speaker Notes:**
  > *"Our physical model computes three accelerations on each agent: goal relaxation steering toward its target, social separation forces pushing nearby agents apart, and radial repulsion from circular or rectangular obstacles."*

---

## Slide 7: Uniform Spatial Grid Partitioning ($\mathcal{O}(N)$ vs $\mathcal{O}(N^2)$)
* **World Mapping:** World ($100 \times 60\text{ m}$) divided into uniform cells matching interaction radius ($C_{\text{size}} = 2.0\text{ m}$).
* **Forward-Star Representation:** Flat `head[cells]` and `next[agents]` arrays avoid dynamic heap allocations.
* **9-Cell Neighborhood Window:** Because cell size $\ge r_{\text{interact}}$, checking the 9 adjacent cells ($3 \times 3$) is guaranteed to capture all neighbors.
* **Work Reduction:** Eliminates **$> 96\%$ of candidate distance checks** ($49.95\text{M} \to 1.52\text{M}$ on 1,000 agents).
* **Speaker Notes:**
  > *"To solve the O(N^2) neighbor problem, we implemented a uniform spatial grid. Because cell size equals the interaction radius, we only need to check the agent's cell and its 8 neighbors. This eliminates over 96% of candidate distance checks."*

---

## Slide 8: OpenMP Implementation & Data Scoping
* **Parallel Work-Sharing Construct:**
  ```c
  #pragma omp parallel for schedule(static) default(none) \
      shared(sim) reduction(+:step_checks, step_neighbors)
  for (int i = 0; i < sim->num_agents; i++) {
      update_single_agent(sim, i, &step_checks, &step_neighbors);
  }
  ```
* **Explicit Data Scoping (`default(none)`):**
  - Loop index `i`: **Private** to each thread on stack.
  - `sim`: **Shared** read-only context.
  - `sim->current_agents`: **Shared** read-only.
  - `sim->next_agents[i]`: **Shared**, disjoint write per iteration.
* **Tree Reduction:** `reduction(+:step_checks)` aggregates metrics with zero atomic lock overhead.
* **Speaker Notes:**
  > *"Here is our OpenMP parallel loop. Notice the use of default(none) for explicit data scoping. The loop index is private, the input array is read-only, and each thread writes strictly to its own assigned index in the output buffer. OpenMP reductions aggregate counters without atomic lock contention."*

---

## Slide 9: Scheduling Policies & Bottleneck Load Balancing
* **Scenario: Narrow Bottleneck Doorway ($N=5,000$ agents, 4 threads)**
  - Extreme spatial clustering near the doorway causes severe computational imbalance.

| Scheduling Policy | Directive Syntax | Runtime (ms) | Speedup vs Seq | Efficiency |
|---|---|:---:|:---:|:---:|
| **Static Scheduling** | `schedule(static)` | 114.82 ms | 1.59× | 39.7% |
| **Dynamic Scheduling**| `schedule(dynamic, 64)` | **79.54 ms** | **2.29×** | **57.3%** |
| **Guided Scheduling** | `schedule(guided, 64)` | 88.06 ms | 2.07× | 51.8% |

* **Insight:** Dynamic scheduling dynamically redistributes crowded doorway chunks, achieving **$44\%$ higher efficiency** than static scheduling!
* **Speaker Notes:**
  > *"When crowds converge on a narrow doorway, agents cluster densely. Static scheduling assigns fixed chunks, so the thread processing the doorway takes 114 ms while other threads sit idle. Dynamic scheduling hands out work in chunks of 64 as threads finish, reducing runtime to 79.5 ms—a 44% efficiency improvement."*

---

## Slide 10: Performance Formulas & Amdahl's Law
* **Speedup ($S$):** $S = T_{\text{sequential}} / T_{\text{parallel}}$
* **Parallel Efficiency ($E$):** $E = (S / P) \times 100\% = (T_{\text{seq}} / (P \times T_{\text{par}})) \times 100\%$
* **Algorithmic Speedup ($S_{\text{algo}}$):** $S_{\text{algo}} = T_{\text{brute}} / T_{\text{grid}}$
* **Amdahl's Law:** Serial fractions (grid reset, pointer swap, barrier synchronisation) establish the theoretical ceiling on speedup.
* **Speaker Notes:**
  > *"We evaluate performance using standard VTU formulas: Speedup, Parallel Efficiency, and Algorithmic Speedup. Amdahl's Law explains why efficiency decreases at higher core counts: serial barriers and memory bus saturation limit maximum achievable speedup."*

---

## Slide 11: Real Strong Scaling Results (12-Core Workstation)
* **Configuration:** $N = 5,000$ agents, $S = 50$ steps, Circular Pillars scenario:

| Mode | Threads ($P$) | Mean Runtime | Median Runtime | Speedup ($S$) | Efficiency ($E$) | Equivalence |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| **Sequential** | 1 | 182.36 ms | 180.18 ms | 1.00× | 100.0% | Reference |
| **Parallel** | 2 | 107.22 ms | 100.23 ms | **1.70×** | **85.0%** | **MATCH** |
| **Parallel** | 4 | 99.34 ms | 101.69 ms | **1.84×** | **45.9%** | **MATCH** |
| **Parallel** | 8 | 53.48 ms | 50.59 ms | **3.41×** | **42.6%** | **MATCH** |

* **Speaker Notes:**
  > *"These are real empirical measurements on our 12-core test machine. On 5,000 agents, 8 threads reduce execution time from 182.36 ms to 53.48 ms, achieving a 3.41× speedup while maintaining exact mathematical equivalence."*

---

## Slide 12: Interactive Web Dashboard Demonstration
* **Live Features:**
  - Interactive HTML5 Canvas rendering pedestrian streams, circular pillars, and doorway bottlenecks.
  - Real-time toggle between individual agent trajectories and spatial density heatmap.
  - Multi-thread scaling sweep with automatic strong-scaling bar chart.
  - Session history table and one-click CSV export.
* **Speaker Notes:**
  > *"Here is our web dashboard. It visualizes real native simulation data returned by the C engine. Examiners can toggle between viewing individual agents or spatial congestion heatmaps, run live thread scaling sweeps, and export benchmark data to CSV."*

---

## Slide 13: Correctness, Sanitizers & Verification Suite
* **Automated Test Harness (`make test`):** 27 automated test assertions, 100% passing.
* **Key Validations:**
  - Seed determinism: identical seeds yield bitwise identical agent positions.
  - Spatial grid vs brute force: detected neighbor counts match 100%; forces match $\le 10^{-6}$.
  - Sequential vs OpenMP: verified identical states across 1, 2, 4, 8 threads ($\le 10^{-6}$).
  - Invariant audits: zero NaN/inf values, zero boundary escapes, zero obstacle overlaps.
* **Sanitizers (`make test-sanitizer`):** AddressSanitizer and UndefinedBehaviorSanitizer confirm **zero memory leaks and zero undefined behaviors**.
* **Speaker Notes:**
  > *"Correctness is verified by 27 automated tests. We proved that spatial grid forces match brute force within 10^-6, and that sequential and OpenMP results match across 1, 2, 4, and 8 threads. AddressSanitizer confirmed zero memory leaks or buffer overflows."*

---

## Slide 14: Practical Applications & Future Scope
* **Real-World Applications:** Pedestrian stadium evacuation, transit terminal design, emergency fire egress planning, game engine AI.
* **Future Work:**
  - SIMD vectorization (`#pragma omp simd`) for inner neighbor loops.
  - Spatial domain decomposition with hybrid MPI + OpenMP for multi-node supercomputers.
  - $A^*$ Navigation Meshes for complex architectural floorplans.
* **Speaker Notes:**
  > *"CrowdFlow's computational patterns apply directly to architectural egress planning and game physics. In future work, we plan to implement SIMD vectorization and hybrid MPI for multi-node supercomputing clusters."*

---

## Slide 15: Conclusion & Summary of Deliverables
* **Project Deliverables:**
  - High-performance modular C11/OpenMP native engine (`crowdflow`).
  - Uniform spatial grid delivering $> 95\%$ candidate check reduction ($\mathcal{O}(N)$).
  - Race-free double-buffered OpenMP parallelization with dynamic load balancing.
  - Zero-dependency web dashboard with HTML5 Canvas heatmap and CSV export.
  - Comprehensive VTU documentation, formal DOCX report, PPTX presentation, and viva guide.
* **Thank you! Questions and viva discussion are welcome.**
* **Speaker Notes:**
  > *"In conclusion, CrowdFlow demonstrates how high-performance algorithms and shared-memory parallel programming unite to solve complex computational physics challenges. Thank you, and I look forward to your questions."*
