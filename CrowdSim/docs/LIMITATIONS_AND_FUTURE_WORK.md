# CrowdFlow Limitations and Future Work

## 1. Scope & Academic Boundaries

CrowdFlow is an educational parallel computing and crowd dynamics platform designed for college and university-level computer science curricula. It models continuous-force pedestrian dynamics, spatial partitioning, and OpenMP multi-core scalability.

### What CrowdFlow IS:
- A high-performance C11/OpenMP simulation implementing goal-seeking steering and social separation forces.
- An algorithmic comparison platform demonstrating $\mathcal{O}(N)$ spatial grids vs $\mathcal{O}(N^2)$ brute force.
- A multi-core performance analysis platform benchmarking static, dynamic, and guided OpenMP scheduling.
- An interactive dashboard displaying real native simulation outputs, spatial heatmaps, and scaling charts.

### What CrowdFlow IS NOT:
- A psychological panic or stampede predictor.
- A commercial building code evacuation verification suite (e.g., Pathfinder, FDS+Evac).
- A GPU/CUDA or distributed-memory MPI cluster application.

---

## 2. Technical Limitations of the Current Implementation

1. **2D Continuous Space Model:** The simulation is strictly planar 2D without multi-story staircases, ramps, or elevation changes.
2. **Simplified Agent Kinematics:** Agents are modeled as circular 2D entities with isotropic personal space. Real humans possess anisotropic visual fields (stronger avoidance in front than behind) and shoulder-twisting articulation.
3. **Single-Node Shared-Memory Scope:** Execution is bounded by the physical core count and memory bandwidth of a single workstation. Distributed multi-node scaling across networks is not supported.
4. **Simple Obstacle Geometry:** Obstacles are restricted to circles and axis-aligned rectangles. Arbitrary convex/concave polygonal meshes with polygon clipping are not currently supported.

---

## 3. Realistic Future Extensions

| Extension | Technical Description | Expected Impact |
|---|---|---|
| **Anisotropic Visual Field** | Scale repulsive forces by $(1 + \cos \theta) / 2$, where $\theta$ is angle between velocity and displacement. | More realistic pedestrian following behavior and corridor lane formation. |
| **SIMD Vectorization (`#pragma omp simd`)** | Align agent structures to 64 bytes and apply AVX-512 vectorization to neighbor distance evaluations. | $2\times$ to $4\times$ throughput increase per core alongside thread parallelism. |
| **Hybrid MPI + OpenMP** | Spatial domain decomposition: partition the 2D world across network nodes using MPI, with OpenMP managing local cores within each node. | Enables scaling to tens of millions of agents across distributed supercomputers. |
| **GPU Acceleration (OpenMP Target Offload / CUDA)** | Offload neighbor queries and integration to massive SIMT GPU streaming multiprocessors. | Extreme throughput on large-scale crowd benchmarks ($N \ge 1,000,000$). |
| **NavMesh Pathfinding ($A^*$)** | Precompute global navigation meshes for complex architectural floorplans with maze-like corridors. | Enables realistic building evacuation simulations around complex obstacles. |
