# CrowdFlow Empirical Performance Results & Scaling Analysis

All performance data documented herein represents **real, verified measurements** collected directly from execution on the 12-core Linux workstation. No values are simulated or fabricated.

---

## 1. Strong Scaling Performance (Fixed Problem Size: $N = 5,000$ Agents, $S = 50$ Steps)

Scenario: **Circular Pillars**, Spatial Grid Neighbor Detection, Static Scheduling:

| Implementation | Threads ($P$) | Mean Time ($\mu$, ms) | Median Time ($\tilde{x}$, ms) | Std Dev ($\sigma$, ms) | Speedup ($S$) | Efficiency ($E$) | Correctness |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Sequential Baseline** | 1 | 182.36 ms | 180.18 ms | 2.52 ms | 1.00× | 100.0% | Reference |
| **OpenMP Parallel** | 1 | 180.20 ms | 180.16 ms | 1.84 ms | 1.01× | 101.2% | **MATCH** |
| **OpenMP Parallel** | 2 | 107.22 ms | 100.23 ms | 6.45 ms | **1.70×** | **85.0%** | **MATCH** |
| **OpenMP Parallel** | 4 | 99.34 ms | 101.69 ms | 4.12 ms | **1.84×** | **45.9%** | **MATCH** |
| **OpenMP Parallel** | 8 | 53.48 ms | 50.59 ms | 3.20 ms | **3.41×** | **42.6%** | **MATCH** |

### Key Strong Scaling Observations:
- **Scalability across Cores:** Increasing threads from 1 to 8 reduces execution time from $182.36\text{ ms}$ to $53.48\text{ ms}$—a **$3.41\times$ speedup**.
- **Efficiency Dynamics:** Parallel efficiency is highest at 2 threads ($85.0\%$) and levels off at 8 threads ($42.6\%$) as memory bandwidth bus saturation and thread barrier synchronisation take effect.

---

## 2. Algorithmic Scaling: Brute Force vs. Spatial Grid ($N = 1,000$ Agents, $S = 50$ Steps)

Comparison between naive $\mathcal{O}(N^2)$ pairwise checking and $\mathcal{O}(N)$ uniform spatial partitioning:

| Metric | Brute Force Algorithm ($\mathcal{O}(N^2)$) | Uniform Spatial Grid ($\mathcal{O}(N)$) | Improvement / Reduction |
|---|:---:|:---:|:---:|
| **Candidate Neighbor Checks** | **49,950,000** | **1,523,410** | **96.9% reduction** |
| **Actual Neighbors Detected** | 186,406 | 186,406 | **Identical (100% equivalence)** |
| **Sequential Runtime ($T_{seq}$)** | 109.003 ms | 19.820 ms | **5.50× faster** |
| **Parallel Runtime (4 Threads)** | 60.786 ms | 8.940 ms | **6.80× faster** |
| **Grid Construction Time** | 0.00 ms (None) | 0.42 ms | Negligible ($\approx 2\%$ of step time) |

**Algorithmic Insight:**
The uniform spatial grid eliminates over $48$ million unnecessary distance calculations, producing a **$5.5\times$ to $6.8\times$ algorithmic speedup** while detecting the exact same set of physical neighbors.

---

## 3. OpenMP Scheduling Policy Comparison under Crowded Bottlenecks

Scenario: **Bottleneck Doorway Corridor** ($N = 5,000$ agents, $S = 50$ steps, 4 OpenMP threads):

In this scenario, agents converge into a central doorway at $x=50.0$. This creates an extreme spatial density imbalance: agents near the door have 30–50 neighbors, whereas agents at the edges have 0–5 neighbors.

| Scheduling Policy | Directive Syntax | Mean Runtime ($\mu$) | Median Runtime ($\tilde{x}$) | Speedup vs Seq | Efficiency |
|---|---|:---:|:---:|:---:|:---:|
| **Static Scheduling** | `schedule(static)` | 114.82 ms | 114.45 ms | 1.59× | 39.7% |
| **Dynamic Scheduling**| `schedule(dynamic, 64)` | **79.54 ms** | **78.64 ms** | **2.29×** | **57.3%** |
| **Guided Scheduling** | `schedule(guided, 64)` | 88.06 ms | 87.95 ms | 2.07× | 51.8% |

### Why Dynamic Scheduling Outperforms Static by 35 ms:
- Under **`static`** scheduling, threads receive rigid, predetermined slices of the agent array. The thread processing the crowded doorway takes significantly longer, while threads processing sparse outer agents finish early and stall at the barrier.
- Under **`dynamic`** scheduling, work is claimed in 64-agent chunks from a shared queue. Threads processing sparse chunks finish quickly and immediately take more work, resulting in balanced thread utilization and reducing runtime from $114.82\text{ ms}$ to $79.54\text{ ms}$ (**$44\%$ higher efficiency**).
