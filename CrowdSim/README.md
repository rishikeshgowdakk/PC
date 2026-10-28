# CrowdSim — Sequential and OpenMP-Based Parallel Particle Movement Simulator

[![Language: C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Parallel API: OpenMP](https://img.shields.io/badge/Parallel_API-OpenMP_4.5%2B-orange.svg)](https://www.openmp.org/)
[![Build: GCC](https://img.shields.io/badge/Compiler-GCC_15.2-green.svg)](https://gcc.gnu.org/)
[![Platform: Linux / POSIX](https://img.shields.io/badge/Platform-Linux%20%7C%20POSIX-lightgrey.svg)](https://www.kernel.org/)
[![Tests: 15/15 Passed](https://img.shields.io/badge/Tests-15%2F15%20Passed-brightgreen.svg)](Makefile)

---

## 1. Project Abstract

**CrowdSim** (*Sequential and OpenMP-Based Parallel Particle Movement Simulator*) is an academic parallel computing project designed for the Visvesvaraya Technological University (VTU) Parallel Computing curriculum.

The application models the movement of multiple independent point particles inside a bounded two-dimensional rectangular environment ($100.0 \times 40.0$ units). Each particle possesses floating-point coordinates `(x, y)` and velocities `(vx, vy)`. Over a sequence of discrete simulation time steps, positions are updated and reflected whenever boundaries are reached.

CrowdSim implements two distinct simulation engines:
1. **Sequential Engine:** An ordinary single-threaded C loop serving as the reference baseline.
2. **Parallel Engine:** A multi-threaded implementation utilizing **OpenMP** shared-memory parallelism (`#pragma omp parallel for schedule(static)`).

Both implementations begin from the exact same deterministic initial state. Upon completion, the program:
- Measures high-resolution wall-clock execution time using `omp_get_wtime()`.
- Calculates **Speedup** ($S = T_{seq} / T_{par}$) and **Parallel Efficiency** ($E = (S / P) \times 100\%$).
- Performs strict floating-point verification confirming that parallel execution preserved exact physical correctness ($\le 10^{-9}$ tolerance).
- Audits that all particles remained strictly within environment boundaries.
- Provides an interactive **Web Dashboard** powered by a zero-dependency Python standard library server and an HTML5 Canvas spatial density heatmap.

> **Academic Disclaimer:** CrowdSim is an educational particle simulation demonstrating shared-memory loop parallelization. It does not model real human psychological crowd behaviour, panic dynamics, personal space, or obstacle pathfinding.

---

## 2. Problem Statement

Simulating the movement of many independent objects requires repeatedly updating their positions over multiple time steps. When the number of objects $N$ or the number of steps $S$ increases, total computational cost scales as $\mathcal{O}(N \times S)$.

Sequential execution on a single CPU core leaves modern multi-core processors largely idle. Because each particle's movement and wall reflection depends strictly on its own state during a given time step, the problem exhibits **embarrassing parallelism** (data independence). CrowdSim investigates how effectively shared-memory multi-core hardware can accelerate this workload using OpenMP directives, while exploring real-world parallel computing trade-offs such as thread creation overhead, memory bandwidth, and Amdahl's Law.

---

## 3. VTU Syllabus Mapping

CrowdSim directly fulfills key laboratory learning outcomes in the VTU Parallel Computing curriculum:

* **Module 1 — Introduction to Parallel Programming & Shared Memory:**
  - Explores shared-memory hardware architectures (MIMD multi-core processors).
  - Demonstrates coordinating worker threads that share a common virtual address space.
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

> **Note:** This project is strictly a shared-memory OpenMP application. It does not implement MPI (distributed memory) or CUDA (GPU acceleration).

---

## 4. Repository Structure

```text
CrowdSim/
├── crowd_sim.c          # Native C11 simulation core with OpenMP
├── server.py            # Local Python HTTP server & subprocess controller
├── Makefile             # Compilation, test automation, and web targets
├── README.md            # This documentation file
├── .gitignore           # Ignores build artifacts and binaries
├── web/                 # Static web dashboard assets
│   ├── index.html       # HTML5 dashboard markup
│   ├── style.css        # Responsive dark-theme styling
│   └── app.js           # Client-side API caller & canvas renderer
├── docs/                # Comprehensive documentation suite
│   ├── PROJECT_OVERVIEW.md
│   ├── TECHNICAL_DOCUMENTATION.md
│   ├── PROJECT_REPORT.md
│   ├── VIVA_QUESTIONS.md        # 70+ structured viva Q&As
│   ├── PRESENTATION_CONTENT.md  # 15 slides with speaker notes
│   ├── DEMO_GUIDE.md            # Speaking script & viva defense
│   ├── TESTING.md               # Full test & benchmark report
│   ├── LIMITATIONS_AND_FUTURE_WORK.md
│   ├── diagrams/        # Mermaid architecture & flowchart sources
│   └── screenshots/     # Genuine browser captures
├── report/              # Word submission document
│   └── CrowdSim_Project_Report.docx
└── presentation/        # PowerPoint presentation
    └── CrowdSim_Project_Presentation.pptx
```

---

## 5. Build, Test, and Launch Instructions

### Prerequisites
- GCC with OpenMP support (`gcc -fopenmp`).
- GNU Make.
- Python 3.8+ (standard library only; no pip packages needed).

### Quick Commands

```bash
# 1. Compile the native simulation binary
make

# 2. Run automated regression test suite (15 test cases)
make test

# 3. Launch the interactive web dashboard
make web
# Open your browser at http://127.0.0.1:8080/

# 4. Run directly from terminal with custom arguments
./crowd_sim --particles 100000 --steps 500 --threads 4

# 5. Clean generated binaries
make clean
```

---

## 6. Real Empirical Benchmark Results

Measured directly on a **12-core Linux workstation** (GCC 15.2.0, `-O2 -fopenmp`):

| Particles ($N$) | Steps ($S$) | Threads ($P$) | Sequential Time ($T_{seq}$) | Parallel Time ($T_{par}$) | Speedup ($S$) | Parallel Efficiency ($E$) | Correctness | Boundaries |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 10,000 | 200 | 1 | 8.960 ms | 8.798 ms | 1.02× | 101.8% | MATCH | PASS |
| 10,000 | 200 | 2 | 6.359 ms | 6.252 ms | 1.02× | 50.9% | MATCH | PASS |
| 10,000 | 200 | 4 | 8.616 ms | 4.752 ms | **1.81×** | 45.3% | MATCH | PASS |
| 10,000 | 200 | 8 | 8.829 ms | 6.034 ms | 1.46× | 18.3% | MATCH | PASS |
| 50,000 | 200 | 1 | 28.567 ms | 21.528 ms | 1.33× | 132.7% | MATCH | PASS |
| 50,000 | 200 | 2 | 24.066 ms | 24.842 ms | 0.97× | 48.4% | MATCH | PASS |
| 50,000 | 200 | 4 | 32.292 ms | 13.812 ms | **2.34×** | 58.5% | MATCH | PASS |
| 50,000 | 200 | 8 | 41.890 ms | 9.100 ms | **4.60×** | **57.5%** | MATCH | PASS |
| 100,000 | 200 | 1 | 63.901 ms | 42.970 ms | 1.49× | 148.7% | MATCH | PASS |
| 100,000 | 200 | 2 | 49.565 ms | 47.913 ms | 1.03× | 51.7% | MATCH | PASS |
| 100,000 | 200 | 4 | 45.616 ms | 29.519 ms | 1.55× | 38.6% | MATCH | PASS |
| 100,000 | 200 | 8 | 43.423 ms | 15.987 ms | **2.72×** | 34.0% | MATCH | PASS |

---

## 7. Performance Formulas

### Speedup ($S$)
$$S = \frac{T_{\text{sequential}}}{T_{\text{parallel}}}$$
Measures the relative performance gain of parallel execution over the single-threaded baseline.

### Parallel Efficiency ($E$)
$$E = \frac{S}{P} \times 100\% = \frac{T_{\text{sequential}}}{P \times T_{\text{parallel}}} \times 100\%$$
Measures how effectively the $P$ allocated hardware threads are utilized ($100\%$ indicates linear scaling).

---

## 8. Automated Test Suite (15/15 Passed)

Run `make test` to execute the full regression suite:
- **T01–T02:** Functional baseline & micro-workload validation.
- **T03–T05:** Multi-thread equivalence (1, 4, 8 threads match sequential reference within $10^{-9}$ tolerance).
- **T06:** Boundary audit (all coordinates remain inside $[0, 100] \times [0, 40]$).
- **T07–T14:** Robust error handling (rejection of zero, negative, non-numeric arguments and unknown flags).
- **T15:** Command-line synopsis verification.

---

## 9. Academic Deliverables Guide

For viva voce examination and report submission:
- **Formal Project Report:** `report/CrowdSim_Project_Report.docx` (DOCX with tables and embedded screenshots) and `docs/PROJECT_REPORT.md` (Markdown).
- **Presentation Slides:** `presentation/CrowdSim_Project_Presentation.pptx` (15 slides with speaker notes) and `docs/PRESENTATION_CONTENT.md`.
- **Viva Voce Guide:** `docs/VIVA_QUESTIONS.md` (70+ questions and answers across 7 core topics).
- **Demonstration Guide:** `docs/DEMO_GUIDE.md` (30s, 2m, and 5m speaking scripts with viva defense tips).
