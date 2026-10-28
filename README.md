# CrowdSim — Sequential and OpenMP-Based Parallel Particle Movement Simulator

## Abstract

CrowdSim simulates the movement of many independent particles inside a bounded
two-dimensional environment.  Each particle has a position and velocity; at
every time step its position is updated and reflected off the walls.

The simulation is implemented twice — once sequentially and once using OpenMP
shared-memory parallelism — and both runs are timed, compared, and validated.
This demonstrates loop-level parallelism, thread-safe computation, and
performance measurement on a straightforward computational workload.

> **Note:** CrowdSim is an educational particle simulator.  It does not model
> real human crowd behaviour, collision, pathfinding, or physics.

---

## Problem Statement

Updating the positions of *N* particles over *S* time steps requires *N × S*
independent calculations.  When *N* is large, this workload becomes
significant.  Because each particle's update depends only on its own state, the
inner loop is *embarrassingly parallel* and maps naturally onto OpenMP threads.

---

## Objectives

| # | Objective |
|---|-----------|
| 1 | Simulate particle movement with wall reflection in a bounded 2-D area. |
| 2 | Implement a sequential (single-threaded) reference version. |
| 3 | Implement a parallel version using OpenMP `parallel for`. |
| 4 | Measure runtime, speedup, and parallel efficiency. |
| 5 | Validate that both implementations produce identical results. |

---

## Syllabus Mapping (VTU Parallel Computing)

### Module 1 — Introduction to Parallel Programming
- Shared-memory programming model.
- Coordinating threads that share an address space.
- Parallel hardware concepts (multi-core CPUs).

### Module 2 — Performance
- Measuring execution time of sequential and parallel programs.
- Speedup and efficiency for MIMD systems.
- Scalability considerations (overhead vs. workload size).

### Module 4 — Shared-Memory Programming with OpenMP
- `#pragma omp parallel for` directive.
- Static loop scheduling (`schedule(static)`).
- Variable scoping — each particle element is accessed by exactly one thread.
- Thread safety — no shared mutable state between loop iterations.

> This project does **not** implement MPI, CUDA, or distributed-memory
> parallelism.

---

## Algorithm

### Sequential

```
for each time step s = 1 … S:
    for each particle i = 1 … N:
        p[i].x += p[i].vx
        p[i].y += p[i].vy
        reflect p[i] off walls
```

### Parallel (OpenMP)

```
for each time step s = 1 … S:
    #pragma omp parallel for schedule(static)
    for each particle i = 1 … N:
        p[i].x += p[i].vx
        p[i].y += p[i].vy
        reflect p[i] off walls
```

The outer loop remains serial because step *s+1* depends on the positions
computed during step *s*.  The inner loop is parallel because particles are
independent.

---

## Requirements

- **GCC** with OpenMP support (`-fopenmp`).
- Standard C library and math library (`-lm`).
- A POSIX-like environment (Linux, macOS, WSL).

No external libraries, frameworks, or runtimes are required.

---

## Build and Run

```bash
# Compile
make

# Run with defaults (50 000 particles, 500 steps, 4 threads)
./crowd_sim

# Custom parameters
./crowd_sim --particles 100000 --steps 500 --threads 4

# Show usage
./crowd_sim --help

# Run automated tests
make test

# Remove compiled binary
make clean
```

---

## Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--particles N` | Number of particles | 50 000 |
| `--steps N` | Number of simulation time steps | 500 |
| `--threads N` | Number of OpenMP threads | 4 |
| `--help` | Print usage information and exit | — |

All numeric arguments must be positive integers.  Invalid values are rejected
with an error message.

---

## Performance Formulas

### Speedup

```
Speedup = T_sequential / T_parallel
```

### Parallel Efficiency

```
Efficiency = Speedup / Number_of_threads × 100%
```

---

## Practical Relevance

Updating many independent objects at each time step is a common pattern in:

- Particle simulations (molecular dynamics, fluid particles).
- Game-engine entity updates.
- Agent-based modelling.
- Monte Carlo methods.

CrowdSim demonstrates the simplest form of this pattern — an embarrassingly
parallel loop — and shows how to apply OpenMP with minimal effort.

---

## Limitations

- **Hardware-dependent results.**  Speedup depends on the number of physical
  cores, CPU frequency, memory bandwidth, cache effects, and system load.
- **Small workloads may be slower in parallel** because of thread-creation and
  synchronisation overhead.
- **No collision detection.**  Particles pass through each other.
- **No real crowd dynamics.**  The simulation is a pedagogical tool, not a
  crowd-prediction system.
- **Single measurement.**  For statistically robust benchmarks, multiple runs
  and warm-up iterations would be needed.

---

## Repository Structure

```
CrowdSim/
├── crowd_sim.c      # Complete C source code
├── Makefile          # Build, run, test, clean targets
├── README.md         # This file
└── .gitignore        # Ignore compiled artefacts
```

---

## License

This project is provided for educational purposes.
