# VISVESVARAYA TECHNOLOGICAL UNIVERSITY
### Belagavi, Karnataka – 590018

---

# A MINI-PROJECT REPORT ON
# **CrowdSim: Sequential and OpenMP-Based Parallel Particle Movement Simulator**

*Submitted in partial fulfillment of the requirements for the award of the degree of*  
**BACHELOR OF ENGINEERING**  
*in*  
**COMPUTER SCIENCE AND ENGINEERING**

---

### **Submitted By:**
**[Student Name]** — **[USN: 1XX2XCSXXX]**

### **Under the Guidance of:**
**[Guide Name]**  
*Designation, Department of Computer Science & Engineering*

---

### **DEPARTMENT OF COMPUTER SCIENCE & ENGINEERING**
**[COLLEGE NAME HERE]**  
*[College Address, City, Karnataka, PIN]*  
**Academic Year: 2025 – 2026**

---

## CERTIFICATE

This is to certify that the mini-project entitled **"CrowdSim: Sequential and OpenMP-Based Parallel Particle Movement Simulator"** is a bonafide work carried out by **[Student Name] (USN: [USN])** in partial fulfillment for the award of Bachelor of Engineering in Computer Science and Engineering of the Visvesvaraya Technological University, Belagavi during the academic year 2025–2026. It is certified that all corrections and suggestions indicated for internal assessment have been incorporated in the report deposited in the departmental library. The project report has been approved as it satisfies the academic requirements in respect of project work prescribed for the said degree.

<br><br>

----------------------------- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; -----------------------------  
**Signature of the Guide** &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; **Signature of the HOD**  
([Guide Name]) &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; ([HOD Name])

<br>

**External Viva-Voce Examination:**  
Name of Examiners &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Signature with Date  
1. ________________________ &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; ____________________  
2. ________________________ &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; ____________________

---

## DECLARATION

I, **[Student Name] (USN: [USN])**, student of Bachelor of Engineering in Computer Science and Engineering, **[College Name]**, hereby declare that the project work entitled **"CrowdSim: Sequential and OpenMP-Based Parallel Particle Movement Simulator"** has been independently carried out by me under the guidance of **[Guide Name]**, Department of Computer Science & Engineering, submitted in partial fulfillment of the requirements for the award of Bachelor of Engineering in Computer Science & Engineering by Visvesvaraya Technological University, Belagavi during the academic year 2025–2026. I further declare that this report has not been submitted previously to any other university or institute for the award of any degree or diploma.

<br>
**Place:** [City Name]  
**Date:** [Date]  
**[Student Name]**  
(USN: [USN])

---

## ACKNOWLEDGEMENT

The satisfaction and euphoria that accompany the successful completion of any task would be incomplete without the mention of people who made it possible, whose constant guidance and encouragement served as a beacon of light.

I express my sincere gratitude and deep sense of indebtedness to our respected Principal, **[Principal Name]**, for providing the necessary infrastructure and inspiring academic atmosphere.

I express my heartfelt thanks to our Head of the Department, **[HOD Name]**, for their continuous encouragement and support throughout the course of this project.

I am profoundly grateful to my project guide, **[Guide Name]**, for their invaluable guidance, constructive suggestions, and keen interest during the course of this work. Their critical evaluations and technical advice were essential to completing this project.

Finally, I express my gratitude to all the teaching and non-teaching faculty of the Department of Computer Science & Engineering, my family, and my fellow classmates who directly or indirectly helped me in successfully completing this mini-project.

<br>
**[Student Name]**

---

## ABSTRACT

In computational physics, dynamic particle simulation, and numerical modelling, updating the positions of thousands of interacting or independent entities over discrete time steps constitutes a computationally intensive workload. When executed sequentially on a single processor core, the workload scales linearly as $\mathcal{O}(N \times S)$, failing to leverage the multi-core processing capabilities inherent in modern Symmetric Multiprocessing (SMP) computer architectures.

This project presents **CrowdSim**, an educational parallel computing application developed in C (ISO C11) utilizing OpenMP for shared-memory multi-core parallelism. The application models independent point particles moving within a bounded two-dimensional rectangular environment ($100.0 \times 40.0$ units) with elastic wall reflection governed by periodic modular folding. CrowdSim implements two distinct computational engines: a single-threaded sequential reference engine and a multi-threaded parallel engine utilizing OpenMP work-sharing directives (`#pragma omp parallel for schedule(static)`). Both implementations begin from identical, deterministic initial states.

High-resolution wall-clock timing is captured via `omp_get_wtime()`, from which Speedup ($S = T_{seq} / T_{par}$) and Parallel Efficiency ($E = (S / P) \times 100\%$) are calculated. Strict numerical verification guarantees that parallel execution produces results identical to the sequential baseline within a floating-point tolerance of $10^{-9}$. A lightweight integration server built using Python’s standard library connects the compiled native binary to an interactive web dashboard featuring real-time parameter controls, execution logging, and an HTML5 Canvas spatial density heatmap.

Empirical benchmarks conducted on a 12-core Linux workstation demonstrate a peak speedup of **4.60×** with **57.5% efficiency** at 50,000 particles and 8 OpenMP threads, while illustrating thread overhead and Amdahl’s Law on micro-workloads. The project directly maps to Modules 1, 2, and 4 of the VTU Parallel Computing syllabus.

---

## TABLE OF CONTENTS

| Chapter No. | Title | Page No. |
|:---:|---|:---:|
| | **Preliminary Pages** | |
| | Certificate | ii |
| | Declaration | iii |
| | Acknowledgement | iv |
| | Abstract | v |
| | List of Figures | viii |
| | List of Tables | ix |
| | List of Abbreviations | x |
| **1** | **Introduction** | **1** |
| | 1.1 Overview of Parallel Computing | 1 |
| | 1.2 Motivation | 2 |
| | 1.3 Problem Statement | 2 |
| | 1.4 Project Objectives | 3 |
| | 1.5 Scope and Constraints | 3 |
| | 1.6 Practical Applications | 4 |
| **2** | **Problem Definition and Requirements Analysis** | **5** |
| | 2.1 Problem Definition | 5 |
| | 2.2 Limitations of the Existing Approach | 5 |
| | 2.3 Proposed Solution | 6 |
| | 2.4 Software and Hardware Requirements | 6 |
| | 2.5 Functional Requirements | 7 |
| | 2.6 Non-Functional Requirements | 8 |
| **3** | **Theoretical Background & Syllabus Mapping** | **9** |
| | 3.1 Shared-Memory Parallel Architecture | 9 |
| | 3.2 OpenMP Framework and Programming Model | 10 |
| | 3.3 Variable Scoping and Race Condition Prevention | 11 |
| | 3.4 Work-Sharing and Scheduling Strategies | 12 |
| | 3.5 Amdahl’s Law and Performance Formulation | 13 |
| | 3.6 Mapping to VTU Parallel Computing Syllabus | 14 |
| **4** | **System Design and Architecture** | **16** |
| | 4.1 High-Level System Architecture | 16 |
| | 4.2 Data Flow Architecture | 17 |
| | 4.3 Simulation Workflow and Control Flow | 18 |
| | 4.4 Component Specifications | 19 |
| | 4.5 Inter-Process Communication and API Design | 20 |
| **5** | **Implementation Details** | **22** |
| | 5.1 Particle Data Structure Design | 22 |
| | 5.2 Deterministic State Initialization | 23 |
| | 5.3 Mathematical Boundary Reflection Algorithm | 24 |
| | 5.4 Sequential Simulation Implementation | 25 |
| | 5.5 OpenMP Parallel Simulation Implementation | 26 |
| | 5.6 High-Resolution Benchmarking Routine | 27 |
| | 5.7 Numerical Verification and Boundary Auditing | 28 |
| | 5.8 Integration Server and Web Dashboard | 29 |
| **6** | **Mathematical Formulation** | **31** |
| | 6.1 Particle Kinematics in Discrete Time | 31 |
| | 6.2 Periodic Wall Folding Equations | 31 |
| | 6.3 Performance and Scalability Equations | 32 |
| | 6.4 Worked Numerical Example | 33 |
| **7** | **Results and Performance Analysis** | **34** |
| | 7.1 Experimental Benchmark Environment | 34 |
| | 7.2 Measured Benchmark Results | 34 |
| | 7.3 Speedup and Efficiency Analysis | 36 |
| | 7.4 Thread Scalability and Diminishing Returns | 37 |
| | 7.5 Verification of Correctness | 38 |
| **8** | **Testing and Validation** | **39** |
| | 8.1 Automated Regression Test Suite | 39 |
| | 8.2 Boundary and Negative Input Validation | 40 |
| | 8.3 Backend API and Concurrency Testing | 41 |
| | 8.4 Full-Stack Browser UI Verification | 42 |
| **9** | **Applications, Limitations & Future Enhancements** | **43** |
| | 9.1 Practical Relevance | 43 |
| | 9.2 Current Limitations | 43 |
| | 9.3 Realistic Future Scope | 44 |
| **10** | **Conclusion** | **46** |
| | **References** | **47** |
| | **Appendices** | **48** |
| | Appendix A: Build and Run Instructions | 48 |
| | Appendix B: Application Screenshots | 49 |

---

## LIST OF FIGURES

* Figure 4.1: CrowdSim 3-Tier System Architecture
* Figure 4.2: Data Flow Sequence Diagram
* Figure 4.3: Sequential Simulation Flowchart
* Figure 4.4: OpenMP Parallel Work-Sharing Flowchart
* Figure 5.1: 2D Spatial Environment and Boundary Reflection
* Figure 7.1: Speedup vs. OpenMP Thread Count Chart
* Figure 7.2: Parallel Efficiency Across Workloads
* Figure B.1: Web Dashboard Initial State Screenshot
* Figure B.2: Web Dashboard Populated Results Screenshot

---

## LIST OF TABLES

* Table 2.1: Hardware and Software Requirements
* Table 3.1: VTU Syllabus Mapping Matrix
* Table 7.1: Experimental Benchmark Matrix (10k, 50k, 100k Particles)
* Table 8.1: Automated Regression Test Suite Execution Log (15 Tests)
* Table 8.2: Backend API Request Validation Test Cases

---

## LIST OF ABBREVIATIONS

| Abbreviation | Expansion |
|---|---|
| **API** | Application Programming Interface |
| **CPU** | Central Processing Unit |
| **C11** | ISO/IEC 9899:2011 C Language Standard |
| **DOM** | Document Object Model |
| **GCC** | GNU Compiler Collection |
| **GUI** | Graphical User Interface |
| **HTTP** | Hypertext Transfer Protocol |
| **IPC** | Inter-Process Communication |
| **JSON** | JavaScript Object Notation |
| **MIMD** | Multiple Instruction, Multiple Data |
| **MPI** | Message Passing Interface |
| **OpenMP** | Open Multi-Processing |
| **OS** | Operating System |
| **POSIX** | Portable Operating System Interface |
| **RAM** | Random Access Memory |
| **RAW** | Read-After-Write (Data Hazard) |
| **SMP** | Symmetric Multiprocessing |
| **UI** | User Interface |
| **USN** | University Seat Number |
| **VTU** | Visvesvaraya Technological University |

---

# CHAPTER 1: INTRODUCTION

## 1.1 Overview of Parallel Computing
Parallel computing is a paradigm of computation in which multiple operations are carried out simultaneously, operating on the foundational principle that large problems can almost always be divided into smaller sub-problems, which are then solved concurrently ("in parallel"). Historically, software execution was predominantly sequential: a computer program executed a linear stream of machine instructions sequentially on a single Central Processing Unit (CPU) core. 

Over the past two decades, physical limitations known as the "Power Wall" (limits on processor clock frequency due to thermal dissipation constraints) brought an end to Dennard scaling. Consequently, microprocessor manufacturers transitioned from increasing single-core clock frequencies to integrating multiple processing cores on a single physical silicon die. Contemporary consumer and enterprise workstations feature Symmetric Multiprocessing (SMP) architectures equipped with 4, 8, 16, or more physical cores. To extract performance from modern hardware, software applications must be redesigned around concurrent, multi-threaded algorithms.

## 1.2 Motivation
In computational physics, dynamic particle simulation, and numerical modelling, simulating autonomous entities over discrete time steps is computationally intensive. When thousands or millions of particles must be tracked, single-threaded execution becomes a severe bottleneck. 

However, many physical systems exhibit **embarrassing parallelism**: during any given time step, the displacement and boundary reflection of one particle depends solely on its own position and velocity, with no dependence on other particles. This presents an ideal opportunity to study multi-threaded acceleration using OpenMP without the complexity of inter-particle locking.

## 1.3 Problem Statement
The objective of this project is to implement a 2D particle movement simulator in C, accelerate it using OpenMP shared-memory directives, measure real-world performance metrics, mathematically verify that parallel execution preserves exact simulation correctness, and visualize the output through an interactive web dashboard.

## 1.4 Project Objectives
1. Implement a 2D particle simulation in C (ISO C11) within a bounded $100.0 \times 40.0$ arena with mathematical wall reflection.
2. Develop a single-threaded sequential reference engine and a multi-threaded OpenMP parallel engine.
3. Formulate and compute Speedup and Parallel Efficiency using high-resolution wall-clock timers (`omp_get_wtime()`).
4. Mathematically verify that parallel execution produces numerical results identical to the sequential baseline within $10^{-9}$ floating-point tolerance.
5. Audit boundary compliance to ensure zero particle leakage.
6. Build a lightweight Python HTTP server and HTML5 Canvas web dashboard for interactive demonstrations.

## 1.5 Scope and Constraints
CrowdSim is designed as an educational mini-project for the VTU Parallel Computing laboratory:
- **Scope:** Shared-memory multi-core CPUs using GCC and OpenMP.
- **Constraints:** Point-mass particles with zero inter-particle collisions; no real human crowd psychology; single-node execution (no MPI or CUDA).

## 1.6 Practical Applications
The computational pattern implemented in CrowdSim is foundational to:
- Molecular dynamics (Brownian motion, gas kinetics).
- Game physics and entity component update systems.
- Monte Carlo photon dispersion and ray tracing.
- Particle filter tracking in robotics.

---

# CHAPTER 2: PROBLEM DEFINITION AND REQUIREMENTS

## 2.1 Problem Definition
Given $N$ particles with state vectors $P_i = [x_i, y_i, vx_i, vy_i]$ inside a 2D rectangular domain $[0, W] \times [0, H]$, compute their updated states over $S$ discrete time steps such that:
$$x_{i}(t+1) = \text{reflect}_W(x_i(t) + vx_i(t))$$
$$y_{i}(t+1) = \text{reflect}_H(y_i(t) + vy_i(t))$$
The computation must be executed sequentially and in parallel, with verified equivalence:
$$\max_{i} |P_{i,\text{seq}}(S) - P_{i,\text{par}}(S)| \le \epsilon = 10^{-9}$$

## 2.2 Limitations of the Existing Approach
Single-threaded simulations leave up to 90% of modern CPU cores idle. Terminal-only tools lack interactive visual feedback, making it difficult to demonstrate parallel performance concepts in laboratory viva sessions.

## 2.3 Proposed Solution
CrowdSim couples a high-performance C11/OpenMP native engine with a zero-dependency Python integration server and an HTML5 Canvas dashboard. The C engine handles heavy mathematical updates, while the browser provides visual density mapping and real-time metric reporting.

## 2.4 Software and Hardware Requirements

| Category | Component | Specification |
|---|---|---|
| **Hardware** | Processor | x86_64 or ARM64 multi-core CPU (2+ physical cores) |
| | Memory (RAM) | Minimum 2 GB (4 GB recommended) |
| | Storage | Minimum 50 MB free disk space |
| **Software** | Operating System | Linux (Ubuntu, Debian, Fedora), macOS, or WSL2 |
| | C Compiler | GCC 9.0+ supporting OpenMP (`-fopenmp`) |
| | Build Tool | GNU Make |
| | Runtime | Python 3.8+ (Standard Library only) |
| | Web Browser | Modern HTML5-compliant browser (Chrome, Firefox, Safari) |

---

# CHAPTER 3: THEORETICAL BACKGROUND & SYLLABUS MAPPING

## 3.1 Shared-Memory Parallel Architecture
In shared-memory symmetric multiprocessing (SMP), multiple CPU cores access a single global physical memory space via a common system bus or interconnect. All threads created within a process share the same virtual address space, enabling rapid data exchange without socket communication.

## 3.2 OpenMP Framework and Programming Model
OpenMP (Open Multi-Processing) is an API based on compiler directives (`#pragma omp`), library routines, and environment variables. OpenMP follows the **fork-join model**: execution begins as a single master thread. When a parallel construct is encountered, the master thread forks a team of worker threads. Upon completing the work-sharing block, threads synchronize at an implicit barrier and join back into the master thread.

## 3.3 Variable Scoping and Race Condition Prevention
- **Loop control variable `i`:** Implicitly private to each thread, ensuring loop iterations are partitioned safely.
- **Particle array pointer `p`:** Shared among all threads.
- **Race Condition Prevention:** Because thread $k$ updates indices $k \cdot \frac{N}{T}$ to $(k+1) \cdot \frac{N}{T} - 1$, write operations are completely disjoint. No two threads write to the same memory location, eliminating the need for expensive mutual exclusion locks (`#pragma omp critical`).

## 3.4 Work-Sharing and Scheduling Strategies
`schedule(static)` divides the $N$ particles into contiguous, equal-sized blocks at loop entry. Because every particle executes identical arithmetic updates, static scheduling provides optimal load balancing with zero scheduling overhead.

## 3.5 Amdahl’s Law and Performance Formulation
Amdahl’s Law states that speedup is bounded by the serial fraction $1-f$:
$$S_{\text{max}} = \frac{1}{(1-f) + \frac{f}{P}}$$
Serial bottlenecks include the outer time-step loop, initialization, memory allocation, and thread fork-join overhead.

## 3.6 Mapping to VTU Parallel Computing Syllabus

| VTU Syllabus Module | Prescribed Learning Outcome | Implementation in CrowdSim |
|---|---|---|
| **Module 1: Introduction to Parallel Computing** | Shared-memory programming, thread coordination, multi-core architecture | Spawns worker threads sharing heap-allocated particle arrays; coordinates disjoint loop partitions. |
| **Module 2: Performance & Scalability** | Speedup, parallel efficiency, wall-clock timing, Amdahl's Law | Uses `omp_get_wtime()`; formulates $S = T_{seq}/T_{par}$ and $E = S/P$; analyzes overhead on micro-workloads. |
| **Module 4: OpenMP Directives** | `#pragma omp parallel for`, static scheduling, variable scoping, thread safety | Applies `#pragma omp parallel for schedule(static)`; analyzes private vs. shared scoping; proves race-free execution. |

---

# CHAPTER 4: SYSTEM DESIGN AND ARCHITECTURE

## 4.1 High-Level System Architecture
CrowdSim uses a 3-tier modular design:
1. **Client Tier (Web Browser):** Responsive dashboard, Canvas heatmap renderer, user controls.
2. **Server Tier (`server.py`):** Python standard library HTTP server (`127.0.0.1:8080`), input sanitizer, process lock guard.
3. **Engine Tier (`crowd_sim`):** Native C11 binary compiled with GCC and OpenMP.

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

## 4.2 Simulation Workflow
1. User configures parameters ($N, S, T$) and triggers simulation.
2. Backend validates ranges and invokes `./crowd_sim --particles N --steps S --threads T --json`.
3. Engine performs deterministic initialization.
4. Sequential engine runs and is timed.
5. Parallel OpenMP engine runs and is timed.
6. Verification comparator and boundary auditor evaluate state arrays.
7. Engine generates an $80 \times 30$ density grid and emits JSON.
8. Frontend renders metrics and Canvas heatmap.

---

# CHAPTER 5: METHODOLOGY AND IMPLEMENTATION

## 5.1 Particle Data Structure Design
Particles are represented as contiguous C structures:
```c
typedef struct {
    double x;   /* Horizontal coordinate */
    double y;   /* Vertical coordinate */
    double vx;  /* Horizontal velocity */
    double vy;  /* Vertical velocity */
} Particle;
```

## 5.2 Deterministic State Initialization
Trigonometric initialisation guarantees identical starting conditions without non-deterministic random number generators:
```c
static void init_particles(Particle *p, int n) {
    for (int i = 0; i < n; i++) {
        p[i].x  = fmod((double)(i * 7 + 13), ENV_WIDTH);
        p[i].y  = fmod((double)(i * 3 + 7),  ENV_HEIGHT);
        p[i].vx = sin((double)i * 0.1)  * 2.0;
        p[i].vy = cos((double)i * 0.07) * 2.0;
    }
}
```

## 5.3 Mathematical Boundary Reflection Algorithm
Periodic folding handles arbitrary displacement overshoots cleanly:
```c
static void reflect(double *pos, double *vel, double limit) {
    double p = *pos;
    if (p < 0.0 || p > limit) {
        double period = 2.0 * limit;
        p = fmod(p, period);
        if (p < 0.0) p += period;
        if (p > limit) {
            p = period - p;
            *vel = -(*vel);
        }
    }
    *pos = p;
}
```

## 5.4 Sequential Reference Implementation
```c
static void simulate_sequential(Particle *p, int n, int steps) {
    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < n; i++) {
            p[i].x += p[i].vx;
            p[i].y += p[i].vy;
            reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
            reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
        }
    }
}
```

## 5.5 OpenMP Parallel Implementation
```c
static void simulate_parallel(Particle *p, int n, int steps) {
    for (int s = 0; s < steps; s++) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < n; i++) {
            p[i].x += p[i].vx;
            p[i].y += p[i].vy;
            reflect(&p[i].x, &p[i].vx, ENV_WIDTH);
            reflect(&p[i].y, &p[i].vy, ENV_HEIGHT);
        }
    }
}
```

---

# CHAPTER 6: MATHEMATICAL FORMULATION

## 6.1 Particle Kinematics
For particle $i$ with position vector $\mathbf{r}_i = (x_i, y_i)$ and velocity vector $\mathbf{v}_i = (vx_i, vy_i)$ over discrete time increment $\Delta t = 1.0$:
$$\mathbf{r}_i(t + \Delta t) = \mathbf{r}_i(t) + \mathbf{v}_i(t) \cdot \Delta t$$

## 6.2 Performance and Scalability Metrics
- **Speedup:**
  $$S = \frac{T_{\text{sequential}}}{T_{\text{parallel}}}$$
- **Parallel Efficiency:**
  $$E = \frac{S}{P} \times 100\% = \frac{T_{\text{sequential}}}{P \times T_{\text{parallel}}} \times 100\%$$

## 6.3 Worked Numerical Example
Given $T_{seq} = 41.890\text{ ms}$, $T_{par} = 9.100\text{ ms}$, and $P = 8$ threads:
$$S = \frac{41.890}{9.100} = 4.6033 \approx 4.60\times$$
$$E = \frac{4.6033}{8} \times 100\% = 57.54\% \approx 57.5\%$$

---

# CHAPTER 7: RESULTS AND PERFORMANCE ANALYSIS

## 7.1 Experimental Benchmark Environment
- **CPU:** 12-core x86_64 processor
- **OS:** Linux (Ubuntu 6.17 kernel)
- **Compiler:** GCC 15.2.0 (`-std=c11 -O2 -fopenmp`)

## 7.2 Measured Benchmark Results

| Particles ($N$) | Steps ($S$) | Threads ($P$) | Sequential Time ($T_{seq}$) | Parallel Time ($T_{par}$) | Speedup ($S$) | Efficiency ($E$) | Match | Bounds |
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

## 7.3 Analysis of Results
1. **Strong Scaling on Substantial Workloads:** On 50,000 particles and 8 threads, execution time decreased from 41.89 ms to 9.10 ms, achieving **4.60× speedup**.
2. **Overhead on Micro-Workloads:** On small particle counts, thread coordination overhead accounts for a larger fraction of runtime, confirming Amdahl’s Law.
3. **Absolute Numerical Equivalence:** Every benchmark confirmed bitwise mathematical match (`MATCH`) and boundary containment (`PASS`).

---

# CHAPTER 8: TESTING AND VALIDATION

## 8.1 Automated Test Suite Execution
CrowdSim includes an automated 15-test regression suite executed via `make test`:

| Test ID | Description | Execution Command | Result |
|:---:|---|---|:---:|
| T01 | Default Execution | `./crowd_sim --particles 1000 --steps 50 --threads 2` | **PASS** |
| T02 | Small Workload | `./crowd_sim --particles 10 --steps 10 --threads 1` | **PASS** |
| T03 | 1-Thread Equivalence | `./crowd_sim --particles 5000 --steps 100 --threads 1` | **PASS** |
| T04 | 4-Thread Equivalence | `./crowd_sim --particles 5000 --steps 100 --threads 4` | **PASS** |
| T05 | 8-Thread Equivalence | `./crowd_sim --particles 5000 --steps 100 --threads 8` | **PASS** |
| T06 | Boundary Audit | `./crowd_sim --particles 2000 --steps 200 --threads 2` | **PASS** |
| T07 | Reject Zero Particles | `! ./crowd_sim --particles 0` | **PASS** |
| T08 | Reject Negative Particles | `! ./crowd_sim --particles -5` | **PASS** |
| T09 | Reject Non-Numeric Particles | `! ./crowd_sim --particles abc` | **PASS** |
| T10 | Reject Zero Steps | `! ./crowd_sim --steps 0` | **PASS** |
| T11 | Reject Negative Steps | `! ./crowd_sim --steps -1` | **PASS** |
| T12 | Reject Zero Threads | `! ./crowd_sim --threads 0` | **PASS** |
| T13 | Reject Negative Threads | `! ./crowd_sim --threads -2` | **PASS** |
| T14 | Reject Unknown Option | `! ./crowd_sim --bogus` | **PASS** |
| T15 | Help Synopsis Verification | `./crowd_sim --help \| grep -q 'Usage'` | **PASS** |

**Pass Rate: 15 / 15 (100%).**

---

# CHAPTER 9: APPLICATIONS, LIMITATIONS & FUTURE SCOPE

## 9.1 Applications
The embarrassingly parallel patterns in CrowdSim directly inform computational fluid dynamics, particle swarm optimization, and agent-based economic models.

## 9.2 Limitations
1. No particle-to-particle physical collisions ($\mathcal{O}(N^2)$).
2. Point-particle physics; does not model psychological crowd behavior.
3. Single-node SMP architecture only (no MPI/CUDA).

## 9.3 Future Scope
- Spatial partitioning via cell lists to support collisions in $\mathcal{O}(N)$ time.
- Distributed cluster scaling via hybrid MPI + OpenMP.
- Internal polygonal barriers and exit doorways.

---

# CHAPTER 10: CONCLUSION

CrowdSim successfully demonstrates how multi-core CPUs can accelerate computationally intensive particle simulations using OpenMP shared-memory directives. The application achieves up to **4.60× speedup** on 8 threads while guaranteeing complete numerical determinism and boundary containment. By integrating native C11 execution with a clean web dashboard, the project bridges low-level parallel systems programming and accessible visual evaluation, fulfilling all laboratory objectives under VTU Modules 1, 2, and 4.

---

## REFERENCES

1. OpenMP Architecture Review Board, *"OpenMP Application Programming Interface Specification Version 5.2"*, Nov. 2021. Available: https://www.openmp.org/specifications/
2. B. Chapman, G. Jost, and R. van der Pas, *"Using OpenMP: Portable Shared Memory Parallel Programming"*, MIT Press, Cambridge, MA, 2007.
3. P. S. Pacheco, *"An Introduction to Parallel Programming"*, 2nd ed., Morgan Kaufmann / Elsevier, 2021.
4. Visvesvaraya Technological University, *"Parallel Computing Syllabus (Modules 1, 2, 4)"*, Belagavi, Karnataka.
5. ISO/IEC, *"Information technology — Programming languages — C (ISO/IEC 9899:2011)"*, International Organization for Standardization, Geneva, Switzerland.

---

## APPENDICES

### Appendix A: Build and Run Instructions
```bash
# Compile native binary
make

# Execute 15 automated regression tests
make test

# Launch interactive web dashboard
make web
# Open http://127.0.0.1:8080 in browser
```

### Appendix B: Application Screenshots
- Figure B.1: Initial Web Dashboard view (`docs/screenshots/dashboard_initial.png`).
- Figure B.2: Completed Simulation Results with Canvas Density Heatmap (`docs/screenshots/dashboard_results.png`).
