# CrowdSim — Limitations and Future Work

## 1. Honest Scope Clarification

CrowdSim is intentionally designed as an **educational parallel computing demonstration** focused on OpenMP shared-memory fundamentals, thread-safe loop parallelisation, wall reflection mathematics, and runtime performance measurement. 

It is **not** a validated psychological crowd-behaviour predictor, architectural evacuation planner, or commercial physics engine.

---

## 2. Current Technical Limitations

### 2.1 Physics & Movement Model Limitations
1. **Zero Particle-to-Particle Collisions:**
   Particles exist as zero-dimensional point masses that pass through each other without collision detection. Introducing inter-particle collision requires an $\mathcal{O}(N^2)$ pairwise distance check or spatial partitioning data structures (e.g., cell lists or quadtrees), which introduces shared mutable state and lock contention outside the scope of this mini-project.
2. **Absence of Cognitive / Crowd Dynamics:**
   The simulation does not implement Helbing’s Social Force Model, Reynolds' boids flocking rules, panic behaviour, or goal-seeking pathfinding (e.g., $A^*$ or navigation meshes).
3. **Rectangular Domain Without Internal Obstacles:**
   The environment is restricted to a simple $100.0 \times 40.0$ bounding rectangle without interior pillars, doors, bottlenecks, or obstacles.

### 2.2 Parallel & Hardware Limitations
1. **Shared-Memory Single-Node Scope:**
   The implementation is strictly limited to OpenMP on single-node symmetric multiprocessing (SMP) systems. It does not utilize MPI for distributed clusters or CUDA/OpenCL for GPU acceleration.
2. **Memory Bandwidth & Cache Contention:**
   At very high particle counts ($N > 10^7$), performance becomes bound by RAM memory bandwidth rather than floating-point computation (memory-bound kernel), causing parallel efficiency to plateau.
3. **Single Timing Run per Request:**
   By design, each simulation request executes one sequential and one parallel pass to keep interactive demonstration latency low ($< 500\text{ ms}$). Rigorous scientific benchmarking typically requires 10 to 50 iterations with warm-up cycles to eliminate OS scheduler jitter.

---

## 3. Realistic Future Enhancements

| Enhancement | Description | Expected Impact |
|---|---|---|
| **Spatial Binning (Cell Lists)** | Partition the 2D arena into regular spatial cells so particles only check neighbours within adjacent cells. | Enables real physical particle collisions with $\mathcal{O}(N)$ time complexity while preserving parallel scalability. |
| **Hybrid MPI + OpenMP** | Decompose the 2D spatial domain across network nodes using MPI, with OpenMP managing threads within each node. | Extends simulation capability to millions of particles across distributed supercomputing clusters. |
| **Interior Obstacles & Exits** | Add internal polygonal walls and doorway vectors to simulate room evacuation bottlenecks. | Demonstrates real-world bottlenecks (e.g. door arching) while exercising conditional branching. |
| **Statistical Multi-Run Engine** | Implement an automated benchmarking mode that runs 20 warm-up runs, discards outliers, and computes standard deviations and confidence intervals. | Provides publication-grade performance characterization. |
| **Dynamic Scheduling Evaluation** | Compare `schedule(static)` against `schedule(dynamic, chunk)` and `schedule(guided)` under uneven spatial densities. | Direct laboratory comparison of OpenMP scheduling trade-offs for Module 4. |
| **SIMD Vectorization (`#pragma omp simd`)** | Combine multi-threading with compiler autovectorization (AVX2/AVX-512) for inner coordinate loops. | Leverages CPU data-level parallelism alongside thread-level parallelism. |
