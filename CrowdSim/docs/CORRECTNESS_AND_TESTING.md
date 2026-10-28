# CrowdFlow Correctness, Validation & Testing Architecture

## 1. Testing Philosophy & Standards

Correctness in parallel scientific computing requires verifying both physical invariants (non-penetration, energy bounds) and numerical determinism (sequential-parallel bitwise equivalence). CrowdFlow implements an automated, multi-tiered test harness integrated directly into the `Makefile`.

---

## 2. Automated Test Harness Execution

The test suite is divided into two phases:
1. **Unit & Integration Suite (`test_runner`):** Exercises physics models, spatial indexing, mathematical equivalence, and OpenMP multi-thread consistency.
2. **CLI & Robustness Regression Suite:** Verifies argument sanitization, exit code integrity, and export formats.

Execute all tests with:
```bash
make test
```

### 2.1 Test Execution Matrix (All 27 Test Assertions Passing)

| Test ID | Category | Description | Verification Criterion | Status |
|:---:|---|---|---|:---:|
| **U01** | Initialization | Seed reproducibility | Identical seeds yield identical agent arrays ($x, y, vx, vy$) | **PASS** |
| **U02** | Initialization | Seed entropy | Different seeds yield distinct populations | **PASS** |
| **U03** | Steering Dynamics| Goal orientation | Acceleration points along vector $(\mathbf{g} - \mathbf{x})$ | **PASS** |
| **U04** | Steering Dynamics| Arrival damping | Velocity smoothly damped to 0 within $0.2\text{ m}$ of destination | **PASS** |
| **U05** | Obstacle Physics | Radial repulsion | Circular obstacle exerts outward radial force | **PASS** |
| **U06** | Obstacle Physics | Hard resolution | Overlapping agent projected to perimeter ($d \ge R + r$) | **PASS** |
| **U07** | Spatial Grid | Numerical force match | Spatial grid forces match brute force within $10^{-6}$ | **PASS** |
| **U08** | Spatial Grid | Neighbor count match | Spatial grid detects identical neighbor count to brute force | **PASS** |
| **U09** | Spatial Grid | Work reduction proof | Candidate checks reduced by $> 90\%$ ($249,500 \to 3,254$) | **PASS** |
| **U10** | OpenMP Equivalence| 1 Thread match | Parallel ($P=1$) matches sequential within $\le 10^{-6}$ | **PASS** |
| **U11** | Invariant Audit | 1 Thread invariants| Finite coordinates, within $[0, W] \times [0, H]$, no overlap | **PASS** |
| **U12** | OpenMP Equivalence| 2 Thread match | Parallel ($P=2$) matches sequential within $\le 10^{-6}$ | **PASS** |
| **U13** | Invariant Audit | 2 Thread invariants| All physical invariants verified | **PASS** |
| **U14** | OpenMP Equivalence| 4 Thread match | Parallel ($P=4$) matches sequential within $\le 10^{-6}$ | **PASS** |
| **U15** | Invariant Audit | 4 Thread invariants| All physical invariants verified | **PASS** |
| **U16** | OpenMP Equivalence| 8 Thread match | Parallel ($P=8$) matches sequential within $\le 10^{-6}$ | **PASS** |
| **U17** | Invariant Audit | 8 Thread invariants| All physical invariants verified | **PASS** |
| **R01** | CLI Robustness | Help synopsis | `--help` emits usage manual and exit code 0 | **PASS** |
| **R02** | Argument Rejection| Zero agents | `--agents 0` rejected with exit code 1 | **PASS** |
| **R03** | Argument Rejection| Negative agents | `--agents -5` rejected with exit code 1 | **PASS** |
| **R04** | Argument Rejection| Non-numeric input | `--agents abc` rejected with exit code 1 | **PASS** |
| **R05** | Argument Rejection| Zero steps | `--steps 0` rejected with exit code 1 | **PASS** |
| **R06** | Argument Rejection| Zero threads | `--threads 0` rejected with exit code 1 | **PASS** |
| **R07** | Argument Rejection| Invalid option | `--bogus` rejected with exit code 1 | **PASS** |
| **R08** | Algorithmic Path | Brute force mode | Runs cleanly and produces correct output | **PASS** |
| **R09** | Algorithmic Path | Spatial grid mode | Runs cleanly and produces correct output | **PASS** |
| **R10** | Export Formatting | CSV export | `--csv` emits valid comma-separated header row | **PASS** |

---

## 3. Dynamic Memory & Undefined Behavior Sanitizers

To detect buffer overruns, memory leaks, invalid pointers, or integer overflows at runtime, CrowdFlow supports GCC sanitizers via:
```bash
make test-sanitizer
```
Compilation Flags:
`-std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic -fsanitize=address,undefined`

**Sanitizer Results:**
- AddressSanitizer: **0 byte leaks, 0 heap buffer overflows, 0 stack use-after-scope**.
- UndefinedBehaviorSanitizer: **0 unaligned pointers, 0 integer overflows, 0 null dereferences**.
