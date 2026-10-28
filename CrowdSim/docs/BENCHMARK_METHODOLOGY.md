# CrowdFlow Benchmark Methodology & Performance Metrics

## 1. Benchmarking Environment

All benchmarks documented in this platform were executed directly on the following verified laboratory host:
- **Processor:** 12-core AMD/Intel x86_64 Symmetric Multiprocessing (SMP)
- **Operating System:** Linux (Kernel 6.17, x86_64)
- **Compiler:** GCC 15.2.0 (`gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`)
- **Compilation Flags:** `-std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic -lm`
- **Timer Resolution:** OpenMP `omp_get_wtime()` with sub-microsecond wall-clock precision.

---

## 2. Timing Boundary Precision

To ensure strict scientific validity:
- **Excluded from Timed Regions:** Memory allocation (`malloc`), population seeding, terminal output, and JSON serialisation.
- **Included in Timed Regions:** Spatial grid construction, neighbor queries, goal steering, obstacle avoidance, force accumulation, and numerical integration across all $S$ simulation time steps.
- **Trial Averaging:** Measurements are repeated over multiple trials ($K \ge 3$), reporting mean ($\mu$), median ($\tilde{x}$), and sample standard deviation ($\sigma$).

---

## 3. Mathematical Metric Definitions

### 3.1 Speedup ($S$)
Speedup quantifies the relative execution time reduction achieved by parallel computing:
$$S = \frac{T_{\text{sequential}}}{T_{\text{parallel}}}$$
where $T_{\text{sequential}}$ is the mean runtime of the single-threaded implementation and $T_{\text{parallel}}$ is the mean runtime of the multi-threaded implementation on identical workloads and seeds.

### 3.2 Parallel Efficiency ($E$)
Parallel efficiency quantifies the percentage of allocated hardware resources effectively utilized:
$$E = \frac{S}{P} \times 100\% = \frac{T_{\text{sequential}}}{P \times T_{\text{parallel}}} \times 100\%$$
where $P$ is the number of active OpenMP worker threads. Ideal linear scaling yields $E = 100\%$.

### 3.3 Algorithmic Speedup ($S_{\text{algo}}$)
Algorithmic speedup measures the performance advantage of spatial partitioning over naive brute force:
$$S_{\text{algo}} = \frac{T_{\text{brute\_force}}}{T_{\text{spatial\_grid}}}$$

---

## 4. Benchmark Categories & Experimental Design

1. **Strong Scaling:** Fixed problem size ($N = 5,000$ agents, $S = 50$ steps), evaluating thread allocations $P \in \{1, 2, 4, 8, 12\}$.
2. **Algorithmic Scaling:** Comparing brute force ($\mathcal{O}(N^2)$) vs spatial grid ($\mathcal{O}(N)$) across problem sizes.
3. **Scheduling Comparison:** Comparing `static`, `dynamic`, and `guided` OpenMP loop scheduling under non-uniform crowding (Bottleneck Doorway scenario).
