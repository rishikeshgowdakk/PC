# CrowdSim — Technical Documentation

## 1. Codebase Structure

The CrowdSim repository is organized as follows:

```
CrowdSim/
├── crowd_sim.c          # Native C11 simulation core with OpenMP
├── server.py            # Local Python HTTP server & subprocess controller
├── Makefile             # Compilation, test automation, and web targets
├── README.md            # Repository introduction and user guide
├── .gitignore           # Ignores build artifacts and binaries
├── web/                 # Static web dashboard assets
│   ├── index.html       # HTML5 dashboard markup
│   ├── style.css        # Responsive dark-theme styling
│   └── app.js           # Client-side API caller & canvas renderer
├── docs/                # Comprehensive documentation suite
│   ├── PROJECT_OVERVIEW.md
│   ├── TECHNICAL_DOCUMENTATION.md
│   ├── PROJECT_REPORT.md
│   ├── VIVA_QUESTIONS.md
│   ├── PRESENTATION_CONTENT.md
│   ├── DEMO_GUIDE.md
│   ├── TESTING.md
│   ├── LIMITATIONS_AND_FUTURE_WORK.md
│   ├── diagrams/        # Mermaid architecture & flowchart sources
│   └── screenshots/     # Genuine browser captures
├── report/              # Word submission document
│   └── CrowdSim_Project_Report.docx
└── presentation/        # PowerPoint presentation
    └── CrowdSim_Project_Presentation.pptx
```

---

## 2. Core Native Simulation (`crowd_sim.c`)

### 2.1 Particle Data Representation
Each particle is represented as a contiguous C structure:

```c
typedef struct {
    double x;   /* Horizontal coordinate (0.0 <= x <= ENV_WIDTH) */
    double y;   /* Vertical coordinate   (0.0 <= y <= ENV_HEIGHT) */
    double vx;  /* Velocity along X-axis (units/step) */
    double vy;  /* Velocity along Y-axis (units/step) */
} Particle;
```

Memory is allocated as a single contiguous array on the heap:
```c
size_t sz = (size_t)num_particles * sizeof(Particle);
Particle *init = malloc(sz);
Particle *seq  = malloc(sz);
Particle *par  = malloc(sz);
```
Contiguous array allocation guarantees cache line locality, minimizing cache misses during spatial loops.

### 2.2 Deterministic Initialisation
To ensure fair benchmarks, both sequential and parallel engines must start from identical initial states without non-deterministic random number generators (e.g., unseeded `rand()`):

```c
static void init_particles(Particle *p, int n)
{
    for (int i = 0; i < n; i++) {
        p[i].x  = fmod((double)(i * 7 + 13), ENV_WIDTH);
        p[i].y  = fmod((double)(i * 3 + 7),  ENV_HEIGHT);
        p[i].vx = sin((double)i * 0.1)  * 2.0;
        p[i].vy = cos((double)i * 0.07) * 2.0;
    }
}
```
Initial states are generated once in `init`, then cloned into `seq` and `par` via `memcpy()`.

### 2.3 Mathematical Wall Reflection Algorithm
Particles that travel past a wall undergo elastic reflection. Rather than simple clamp or single-bounce logic (which fails when velocities exceed the boundary width), CrowdSim implements a robust periodic folding algorithm:

```c
static void reflect(double *pos, double *vel, double limit)
{
    double p = *pos;
    if (p < 0.0 || p > limit) {
        double period = 2.0 * limit;
        p = fmod(p, period);
        if (p < 0.0)
            p += period;
        if (p > limit) {
            p = period - p;
            *vel = -(*vel);
        }
    }
    *pos = p;
}
```
This guarantees that for any displacement $\Delta x$, the resulting coordinate $x \in [0, \text{limit}]$ and velocity vector direction are physically consistent.

### 2.4 Sequential Reference Engine
```c
static void simulate_sequential(Particle *p, int n, int steps)
{
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
The outer loop indexes discrete time steps $s \in [0, \text{steps}-1]$. The inner loop sequentially updates each particle $i \in [0, n-1]$.

### 2.5 OpenMP Parallel Engine
```c
static void simulate_parallel(Particle *p, int n, int steps)
{
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

#### Parallel Scoping & Thread Safety Analysis:
- **`#pragma omp parallel for`**: Directs the OpenMP runtime to fork a team of threads to execute iterations of the loop in parallel.
- **`schedule(static)`**: Divides the $N$ particles into contiguous, equal-sized chunks of size approximately $N/T$, assigned deterministically to each thread. Because all particles incur nearly identical arithmetic workload per step, static scheduling provides optimal load balancing with zero scheduling overhead.
- **Variable Scoping**:
  - `i`: Loop control variable — **implicitly private** to each thread.
  - `p`: Array pointer — **shared**, but thread $T_k$ writes strictly to indices $i \in [k \cdot \frac{N}{T}, (k+1) \cdot \frac{N}{T} - 1]$. No two threads ever access the same memory location.
  - `s`: Outer time step — **private/serialized** outside the parallel region.
- **Absence of Critical Sections**: Because memory accesses are completely disjoint, no mutual exclusion locks, critical sections, or atomic directives are required, maximizing parallel throughput.

### 2.6 High-Resolution Benchmarking
Timing utilizes the OpenMP wall-clock timer `omp_get_wtime()`, returning double-precision seconds with sub-microsecond resolution:
```c
double t_seq_start = omp_get_wtime();
simulate_sequential(seq, num_particles, num_steps);
double t_seq_end   = omp_get_wtime();
double t_seq_ms    = (t_seq_end - t_seq_start) * 1000.0;
```
Timing regions strictly encapsulate computation loops; memory allocation, deterministic initialisation, and console/JSON I/O are excluded.

### 2.7 Verification Routines
1. **State Comparator (`compare_results`)**:
   ```c
   fabs(a[i].x  - b[i].x)  > TOLERANCE ||
   fabs(a[i].y  - b[i].y)  > TOLERANCE ||
   fabs(a[i].vx - b[i].vx) > TOLERANCE ||
   fabs(a[i].vy - b[i].vy) > TOLERANCE
   ```
   Evaluates equality within floating-point tolerance $\epsilon = 10^{-9}$.
2. **Boundary Validation (`validate_bounds`)**:
   Verifies that every particle coordinate satisfies:
   $$-\epsilon \le x \le \text{ENV\_WIDTH} + \epsilon \quad \text{and} \quad -\epsilon \le y \le \text{ENV\_HEIGHT} + \epsilon$$

---

## 3. Backend Integration Server (`server.py`)

### 3.1 HTTP Server Architecture
The server inherits from Python's standard `http.server.SimpleHTTPRequestHandler` without external third-party dependencies.

### 3.2 API Specification
* **Endpoint**: `POST /api/simulate`
* **Content-Type**: `application/json`
* **Request Payload**:
  ```json
  {
    "particles": 10000,
    "steps": 100,
    "threads": 4
  }
  ```
* **Response Payload (HTTP 200)**:
  ```json
  {
    "particles": 10000,
    "steps": 100,
    "threads": 4,
    "seq_time_ms": 7.835,
    "par_time_ms": 5.090,
    "speedup": 1.5393,
    "efficiency": 38.48,
    "match": true,
    "bounds": true,
    "env_width": 100.0,
    "env_height": 40.0,
    "grid_cols": 80,
    "grid_rows": 30,
    "density": [[...], [...]]
  }
  ```

### 3.3 Security & Concurrency Protections
1. **Loopback Binding**: The socket explicitly binds to `127.0.0.1` (`HOST = "127.0.0.1"`), preventing external network exposure.
2. **Strict Range Validation**:
   - `particles`: Integer, $[1, 5\,000\,000]$.
   - `steps`: Integer, $[1, 1\,000\,000]$.
   - `threads`: Integer, $[1, 64]$.
3. **Safe Subprocess Execution**: Arguments are passed as an explicit list `[CROWD_SIM_BIN, "--particles", str(p), ...]` directly to `subprocess.run(shell=False)`, entirely preventing shell injection vulnerabilities.
4. **Process Concurrency Lock**: A non-blocking `threading.Lock()` ensures that only one simulation executes at any instant. Concurrent requests receive HTTP `429 Too Many Requests`.
5. **Execution Timeout**: Enforces a strict 120-second timeout to kill runaway processes.

---

## 4. Web Frontend (`web/`)

### 4.1 HTML5 Canvas Density Heatmap
Rather than transmitting and rendering millions of individual DOM elements or raw coordinates (which would degrade browser performance), `crowd_sim` bins final coordinates into an $80 \times 30$ spatial density grid. `app.js` renders this grid on an HTML5 `<canvas>` using bilinear colour scaling:
- $\text{density} = 0$: Deep background `#0d1018`
- $0 < \text{density} \le 0.25$: Deep navy blue
- $0.25 < \text{density} \le 0.50$: Mid-range cyan
- $0.50 < \text{density} \le 0.75$: Soft sky blue
- $0.75 < \text{density} \le 1.00$: Vibrant yellow

### 4.2 Error & Loading States
The UI implements reactive state management:
- Buttons are disabled and a CSS keyframe spinner is displayed during active simulations.
- Connection failures, validation errors, and subprocess exceptions are reported in styled alerts.
- Old metrics are cleared before every new execution to prevent misattribution.
