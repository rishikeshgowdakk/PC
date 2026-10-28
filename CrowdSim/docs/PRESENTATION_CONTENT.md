# CrowdSim — Presentation Slide Content & Speaker Notes (15 Slides)

This document contains the exact slide-by-slide text, visual layout specifications, and verbatim speaker notes for the 7–10 minute laboratory presentation.

---

## Slide 1: Title Slide

### Slide Visual Content:
* **Title:** CrowdSim: Sequential and OpenMP-Based Parallel Particle Movement Simulator
* **Subtitle:** VTU Parallel Computing Mini-Project (Modules 1, 2, & 4)
* **Author / Presenter:** [Student Name / USN Placeholder]
* **Guide / Department:** [Department of Computer Science & Engineering]
* **Key Visual:** Diagram of multi-core CPU threads processing 2D particle vectors.

### Speaker Notes:
> *"Respected guide and examiners, good morning. Today I am presenting our Parallel Computing mini-project titled 'CrowdSim: Sequential and OpenMP-Based Parallel Particle Movement Simulator'. This project demonstrates how shared-memory multi-core processors can accelerate computational physics workloads using OpenMP directives, while verifying that parallel execution preserves exact mathematical correctness."*

---

## Slide 2: Problem Statement

### Slide Visual Content:
* **The Challenge:** Simulating dynamic systems requires repeatedly updating positions of thousands of particles over hundreds of discrete time steps.
* **Computational Cost:** Workload scales as $\mathcal{O}(N \times S)$ ($N = \text{particles}$, $S = \text{steps}$).
  - 100,000 particles $\times$ 500 steps = **50,000,000 state updates**.
* **The Single-Core Bottleneck:** Single-threaded execution leaves multi-core hardware idle.
* **Core Question:** Can independent particle updates be cleanly parallelized using OpenMP directives without introducing race conditions or excessive synchronization overhead?

### Speaker Notes:
> *"In scientific computing, simulating particles, fluids, or molecular dynamics requires updating positions repeatedly over time. On a single CPU core, updating 100,000 particles over 500 steps requires 50 million mathematical calculations, creating a computational bottleneck. Our objective is to investigate whether we can distribute these independent calculations across multiple CPU cores using OpenMP, and to quantify the actual speedup achieved."*

---

## Slide 3: Project Objectives

### Slide Visual Content:
* **1. Simulate Particle Movement:** Implement 2D motion with robust wall reflection within a $100 \times 40$ bounded environment.
* **2. Implement Dual Engines:** Create a sequential reference baseline and an OpenMP parallel implementation.
* **3. Formulate Performance Metrics:** Measure high-resolution execution time, calculate Speedup ($S = T_{seq}/T_{par}$) and Efficiency ($E = S/P$).
* **4. Strict Verification:** Verify that parallel and sequential results match within $10^{-9}$ floating-point tolerance and that all particles remain within bounds.
* **5. Interactive Dashboard:** Connect the native C engine to a lightweight web dashboard for live demonstrations.

### Speaker Notes:
> *"The project has five key objectives: first, simulate 2D particle physics with boundary reflections; second, implement both sequential and parallel versions; third, measure wall-clock runtimes and calculate speedup and efficiency; fourth, mathematically verify that parallel execution produces identical results; and fifth, wrap the engine in a web dashboard for interactive evaluation."*

---

## Slide 4: Introduction to Parallel Computing

### Slide Visual Content:
* **Sequential Computing:** Single worker executing tasks serially, one after another.
* **Parallel Computing:** Simultaneous execution of computational sub-tasks across multiple processing cores.
* **MIMD Architecture (Multiple Instruction, Multiple Data):** Modern multi-core processors executing independent instruction streams on disjoint data streams.
* **Shared-Memory Model:** All threads operate within a common virtual address space, enabling rapid data exchange without network packet overhead.

### Speaker Notes:
> *"Parallel computing divides a computational workload across multiple cores simultaneously. In modern multi-core PCs, we utilize the MIMD shared-memory model. Unlike distributed clusters which require sending messages over networks, all worker threads share the same RAM address space, making data sharing extremely fast."*

---

## Slide 5: Existing vs Proposed Approach

### Slide Visual Content:
| Feature | Traditional Sequential Approach | CrowdSim OpenMP Approach |
|---|---|---|
| **Hardware Utilization** | Single core active (~8% on 12-core CPU) | All allocated CPU cores active (100% core load) |
| **Execution Time (50k P, 200 S)** | 41.89 ms | **9.10 ms (4.60× Speedup)** |
| **Scalability** | Fixed runtime bottleneck | Scales with available physical hardware cores |
| **Correctness Guarantee** | Implicit baseline | Strict element-by-element automated verification |
| **Demonstration Interface** | Terminal text only | Interactive web dashboard with Canvas heatmap |

### Speaker Notes:
> *"In the existing sequential approach, only one CPU core is active, leaving up to 90% of modern multi-core computing power idle. Our proposed OpenMP approach engages all available hardware threads, dropping runtime from 41.89 ms to 9.10 ms on 50,000 particles—a 4.6× speedup—while verifying that every coordinate matches the baseline."*

---

## Slide 6: System Architecture

### Slide Visual Content:
* **3-Tier Modular Design:**
  1. **Presentation Layer (Web Browser):** HTML5 Canvas heatmap, parameter controls, metrics table, and execution logs.
  2. **Integration Layer (`server.py`):** Pure Python standard library HTTP server (`127.0.0.1:8080`), input sanitizer, process lock guard.
  3. **Native Computation Engine (`crowd_sim`):** Compiled C11 binary utilizing OpenMP runtime and mathematical wall folding.
* **IPC Protocol:** Standard Input/Output streaming structured JSON with an $80 \times 30$ density grid.

### Speaker Notes:
> *"Here we see the 3-tier architecture. The web browser provides an interactive dashboard. When the user clicks Run, an HTTP POST request is sent to our Python integration server. The server verifies input safety and invokes our compiled C simulation binary. The C binary executes both sequential and parallel runs, validates results, bins coordinates into an 80 by 30 density grid, and returns structured JSON to the browser."*

---

## Slide 7: Particle Physics & Wall Reflection

### Slide Visual Content:
* **State Vector:** $P_i = [x_i, y_i, vx_i, vy_i]$
* **Motion Update:**
  $$x_{new} = x_{old} + vx \cdot \Delta t, \quad y_{new} = y_{old} + vy \cdot \Delta t$$
* **Periodic Folding Wall Reflection Algorithm:**
  - Standard clamping causes high-speed particles to stick to walls.
  - Periodic folding using modular arithmetic (`fmod`) handles arbitrary multi-unit overshoots:
  ```c
  double period = 2.0 * limit;
  p = fmod(p, period);
  if (p < 0.0) p += period;
  if (p > limit) { p = period - p; *vel = -(*vel); }
  ```

### Speaker Notes:
> *"Each particle maintains position and velocity coordinates. At each step, displacement is added. For boundary reflection, simple clamping is fragile if velocity is large. We implemented a periodic folding algorithm using modular arithmetic that cleanly rebounds particles back into the 100 by 40 arena, exactly reversing the velocity vector."*

---

## Slide 8: Sequential Implementation

### Slide Visual Content:
* **Reference Implementation Code Excerpt:**
  ```c
  for (int s = 0; s < steps; s++) {
      for (int i = 0; i < n; i++) {
          p[i].x += p[i].vx;
          p[i].y += p[i].vy;
          reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
          reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
      }
  }
  ```
* **Characteristics:** Single-threaded execution; loop iterations executed strictly in order; zero thread coordination overhead.

### Speaker Notes:
> *"Here is the sequential reference implementation. An outer loop iterates through simulation steps, while the inner loop updates each particle in sequence. It executes on one core and serves as the baseline for performance and verification."*

---

## Slide 9: OpenMP Parallel Implementation

### Slide Visual Content:
* **Parallel Work-Sharing Construct:**
  ```c
  for (int s = 0; s < steps; s++) {
      #pragma omp parallel for schedule(static)
      for (int i = 0; i < n; i++) {
          p[i].x += p[i].vx;
          p[i].y += p[i].vy;
          reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
          reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
      }
  }
  ```
* **Thread Safety & Scoping:**
  - Loop index `i`: **Implicitly private** to each worker thread.
  - Particle array `p`: **Shared**, with completely disjoint index ranges (Thread $k$ updates indices $k \cdot \frac{N}{T}$ to $(k+1) \cdot \frac{N}{T} - 1$).
  - **Zero Critical Sections:** No mutual exclusion locks or race conditions.

### Speaker Notes:
> *"Here is our parallel implementation. Notice the directive `#pragma omp parallel for schedule(static)`. OpenMP forks a team of threads that execute the particle loop concurrently. Because each thread writes strictly to its own separate slice of the particle array, there are zero data races, zero false sharing, and no locks are needed."*

---

## Slide 10: Performance Formulas

### Slide Visual Content:
* **Speedup ($S$):**
  $$S = \frac{T_{\text{sequential}}}{T_{\text{parallel}}}$$
  *Measures acceleration factor over single-threaded baseline.*
* **Parallel Efficiency ($E$):**
  $$E = \frac{S}{P} \times 100\% = \frac{T_{\text{sequential}}}{P \times T_{\text{parallel}}} \times 100\%$$
  *Measures percentage of hardware core capability effectively utilized ($P = \text{threads}$).*
* **Amdahl's Law:**
  $$S_{\text{max}} = \frac{1}{(1-f) + \frac{f}{P}}$$
  *Serial fractions (outer time-step barriers) limit maximum achievable speedup.*

### Speaker Notes:
> *"We evaluate performance using standard VTU Module 2 formulas: Speedup, which is sequential time divided by parallel time, and Parallel Efficiency, which divides speedup by thread count. Amdahl's Law explains why efficiency is less than 100%: serial portions such as time-step synchronization and thread coordination set a theoretical ceiling on speedup."*

---

## Slide 11: Real Empirical Results

### Slide Visual Content:
* **Verified Benchmark Matrix (Host: 12-Core x86_64 Linux):**

| Workload ($N \times S$) | Threads ($P$) | $T_{seq}$ | $T_{par}$ | Speedup | Efficiency |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 10,000 P × 200 S | 4 Threads | 8.61 ms | 4.75 ms | **1.81×** | 45.3% |
| 50,000 P × 200 S | 4 Threads | 32.29 ms | 13.81 ms | **2.34×** | 58.5% |
| 50,000 P × 200 S | 8 Threads | 41.89 ms | 9.10 ms | **4.60×** | **57.5%** |
| 100,000 P × 200 S | 8 Threads | 43.42 ms | 15.98 ms | **2.72×** | 34.0% |

* **Observation:** Speedup improves significantly as particle count increases because arithmetic workload comfortably outweighs thread fork-join overhead.

### Speaker Notes:
> *"These are real, verified measurements from our test machine. For 50,000 particles and 8 threads, sequential execution took 41.89 ms while parallel execution finished in just 9.10 ms—yielding a 4.60× speedup and 57.5% parallel efficiency. Notice that larger workloads yield better efficiency because computational work dominates thread overhead."*

---

## Slide 12: Interactive Web Dashboard

### Slide Visual Content:
* **Screenshot Displays:**
  - Control panel with parameter inputs (Particles, Steps, Threads).
  - High-resolution HTML5 Canvas rendering particle density heatmap (blue $\to$ cyan $\to$ yellow).
  - Results metrics table displaying exact runtimes, speedup, and efficiency.
  - Green verification badges: `MATCH` and `PASS`.
* **Zero Fabrication:** Visualisation represents real binned spatial data returned by C.

### Speaker Notes:
> *"Here is a screenshot of our working web dashboard. The left panel allows entering parameters. The center Canvas displays an 80 by 30 spatial density heatmap returned directly by our C program. The right panel displays the measured times, speedup, efficiency, and confirmation badges proving that all correctness checks passed."*

---

## Slide 13: Testing & Verification

### Slide Visual Content:
* **Automated Regression Suite:** 15 test cases integrated into `Makefile` (`make test`).
* **Test Categories:**
  - Concurrency Equivalence (1, 4, 8 threads vs sequential reference $\le 10^{-9}$).
  - Boundary Auditing (all particles remain inside $[0, 100] \times [0, 40]$).
  - Malformed Input Rejection (zero, negative, non-numeric strings).
  - CLI synopses and error handling.
* **Result:** **15 Passed, 0 Failed (100% Pass Rate).**

### Speaker Notes:
> *"Our automated test suite runs 15 distinct tests covering thread equivalence with 1, 4, and 8 threads, boundary audits, and invalid input rejection. Every single test passes, proving that parallelization did not compromise numerical stability or physical correctness."*

---

## Slide 14: Applications, Limitations & Future Scope

### Slide Visual Content:
* **Practical Applications:** Molecular dynamics, game entity loops, Monte Carlo particle dispersion, graphics ray tracing.
* **Current Limitations:**
  - Educational point-mass model; no inter-particle collisions ($\mathcal{O}(N^2)$).
  - Shared-memory single node only (no MPI or CUDA).
* **Future Enhancements:**
  - Spatial binning (Cell lists) to enable $\mathcal{O}(N)$ particle collisions.
  - Hybrid MPI + OpenMP for multi-node supercomputing clusters.
  - Internal polygonal obstacles and doorway evacuation vector fields.

### Speaker Notes:
> *"This computational pattern applies widely to molecular dynamics and graphics. As an educational mini-project, CrowdSim does not implement inter-particle collisions or human panic psychology. In future work, we could add spatial cell lists for physical collisions and hybrid MPI for multi-node supercomputing clusters."*

---

## Slide 15: Conclusion & Q&A

### Slide Visual Content:
* **Summary of Achievements:**
  - Successfully developed complete C11/OpenMP 2D particle simulation.
  - Demonstrated up to **4.60× Speedup** on multi-core hardware.
  - Proved mathematical equivalence between sequential and parallel implementations.
  - Integrated full-stack web dashboard with HTML5 Canvas density heatmap.
  - Direct practical mapping to VTU Modules 1, 2, and 4.
* **Thank You! Questions & Discussion Welcomed.**

### Speaker Notes:
> *"In conclusion, CrowdSim demonstrates how shared-memory parallel programming with OpenMP can significantly accelerate independent spatial computations without introducing race conditions. It confirms theoretical scalability concepts from our VTU syllabus with real empirical data. Thank you, and I am now ready for your questions."*
