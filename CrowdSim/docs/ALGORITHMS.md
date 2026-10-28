# CrowdFlow Algorithmic Foundations & Complexity Analysis

## 1. Crowd Dynamics & Kinematic Steering Model

CrowdFlow models agent movement using a generalized continuous-force velocity relaxation framework inspired by Helbing’s Social Force model, optimized for high numerical stability and thread-safe evaluation.

### 1.1 Goal-Seeking Steering Force
For an agent $i$ with position $\mathbf{x}_i$, velocity $\mathbf{v}_i$, destination $\mathbf{g}_i$, and preferred speed $v_{\text{pref}}$, the desired velocity vector $\mathbf{v}_{\text{des}}$ points directly toward the destination:
$$\mathbf{d}_i = \mathbf{g}_i - \mathbf{x}_i, \quad d_i = \|\mathbf{d}_i\|$$
$$\hat{\mathbf{n}}_{\text{goal}} = \frac{\mathbf{d}_i}{d_i}$$
$$\mathbf{v}_{\text{des}} = v_{\text{pref}} \cdot \hat{\mathbf{n}}_{\text{goal}}$$

The goal-directed acceleration relaxes current velocity toward desired velocity over relaxation constant $\tau = 0.5\text{ s}$:
$$\mathbf{F}_{\text{goal}} = \frac{\mathbf{v}_{\text{des}} - \mathbf{v}_i}{\tau}$$

When an agent approaches within $0.2\text{ m}$ of its goal ($d_i < 0.2$), velocity is smoothly damped to zero:
$$\mathbf{F}_{\text{goal}} = -\frac{\mathbf{v}_i}{\tau}$$

---

## 2. Inter-Agent Local Collision Avoidance

To prevent agents from overlapping while preserving smooth passing behavior, agents exert repulsive separation forces on nearby neighbors within an interaction radius $r_{\text{interact}} = 2.0\text{ m}$.

For any neighbor $j$ with displacement vector $\mathbf{x}_{ij} = \mathbf{x}_i - \mathbf{x}_j$ and distance $d_{ij} = \|\mathbf{x}_{ij}\|$:

### 2.1 Contact Overlap ($d_{ij} < r_i + r_j$)
When agent physical boundaries touch or overlap, a stiff linear elastic contact force acts to push them apart:
$$\text{overlap} = (r_i + r_j) - d_{ij}$$
$$F_{\text{rep}} = 2 A \cdot (1.0 + 5.0 \cdot \text{overlap})$$

### 2.2 Social Distance Decay ($r_i + r_j \le d_{ij} < r_{\text{interact}}$)
Outside physical contact, social repulsion decays exponentially with distance:
$$\text{gap} = d_{ij} - (r_i + r_j)$$
$$F_{\text{rep}} = A \cdot \exp\left(-\frac{\text{gap}}{B}\right)$$
where $A = 12.0\text{ N/kg}$ and $B = 0.8\text{ m}$.

The net agent-agent repulsive acceleration is the vector sum over all neighbors:
$$\mathbf{F}_{\text{agents}} = \sum_{j \in \mathcal{N}_i} F_{\text{rep}} \cdot \frac{\mathbf{x}_{ij}}{d_{ij}}$$

---

## 3. Obstacle Collision & Penetration Resolution

CrowdFlow supports circular pillars and axis-aligned rectangular barriers (AABB).

### 3.1 Circular Pillars (Center $\mathbf{c}$, Radius $R$)
- **Repulsive Force:** When distance $d = \|\mathbf{x} - \mathbf{c}\| < R + r_{\text{agent}} + \text{margin}$:
  $$\mathbf{F}_{\text{obs}} = k_{\text{obs}} \left(\frac{R + r_{\text{agent}} + \text{margin} - d}{\text{margin}}\right) \frac{\mathbf{x} - \mathbf{c}}{d}$$
- **Hard Projection:** If numerical integration allows an agent to penetrate inside the pillar ($d < R + r_{\text{agent}}$):
  $$\mathbf{x} \leftarrow \mathbf{c} + (R + r_{\text{agent}}) \frac{\mathbf{x} - \mathbf{c}}{d}$$
  Normal velocity component is neutralized: $\mathbf{v} \leftarrow \mathbf{v} - (\mathbf{v} \cdot \hat{\mathbf{n}})\hat{\mathbf{n}}$.

### 3.2 Axis-Aligned Rectangular Barriers ($[x_{\min}, y_{\min}, x_{\max}, y_{\max}]$)
- **Closest Point Projection:** The closest point $\mathbf{q}$ on the rectangle to agent $\mathbf{x}$ is:
  $$q_x = \text{clamp}(x, x_{\min}, x_{\max}), \quad q_y = \text{clamp}(y, y_{\min}, y_{\max})$$
- Distance $d = \|\mathbf{x} - \mathbf{q}\|$. Force acts along unit vector $\frac{\mathbf{x} - \mathbf{q}}{d}$.
- Penetration is resolved by projecting the agent outward to the nearest exterior face.

---

## 4. Algorithmic Complexity: Brute Force vs. Spatial Partitioning Grid

### 4.1 Naive Brute Force Algorithm
For every agent $i \in [0, N-1]$, check all other agents $j \in [0, N-1]$ ($j \neq i$):
- **Candidate Distance Checks:** $\frac{N(N-1)}{2} = \mathcal{O}(N^2)$
- At $N = 10,000$ agents: **$49,995,000$ distance checks per time step**.
- At $N = 50,000$ agents: **$1,249,975,000$ distance checks per time step**.
- *Scaling:* Becomes computationally intractable as $N$ grows beyond several thousand agents.

### 4.2 Uniform Spatial Partitioning Grid
The 2D world ($100 \times 60\text{ m}$) is partitioned into a uniform grid of cells, where cell size $C_{\text{size}} = r_{\text{interact}} = 2.0\text{ m}$:
$$\text{cols} = \lceil 100 / 2.0 \rceil = 50, \quad \text{rows} = \lceil 60 / 2.0 \rceil = 30, \quad \text{Total Cells} = 1500$$

- **Grid Construction ($\mathcal{O}(N)$):** Each agent is mapped to cell $(c_x, c_y)$ and prepended to the cell's linked list using a flat `head`/`next` array in a single $\mathcal{O}(N)$ pass.
- **Neighbor Query ($\mathcal{O}(1)$ average):** Because $C_{\text{size}} \ge r_{\text{interact}}$, any agent within interaction radius of $(x, y)$ is mathematically guaranteed to reside either in the agent's cell or in one of the 8 adjacent neighboring cells (a $3 \times 3$ cell window).
- **Candidate Distance Checks:** Only agents residing in the 9 adjacent cells are evaluated:
  $$\text{Checks per agent} \approx 9 \times \rho_{\text{local}} \ll N$$
- *Empirical Work Reduction:* On $N = 5,000$ agents, the spatial grid reduces candidate checks from $24.9 \times 10^6$ to under $700,000$ per step—a **$> 97\%$ reduction in unnecessary arithmetic work**.

---

## 5. Numerical Integration & State Clamping

Given net force $\mathbf{F} = \mathbf{F}_{\text{goal}} + \mathbf{F}_{\text{agents}} + \mathbf{F}_{\text{obs}}$:
1. **Acceleration Clamping:** $\|\mathbf{F}\| \le a_{\max} = 5.0\text{ m/s}^2$
2. **Velocity Update:** $\mathbf{v}(t + \Delta t) = \mathbf{v}(t) + \mathbf{F} \cdot \Delta t$
3. **Speed Clamping:** $\|\mathbf{v}(t + \Delta t)\| \le v_{\max} = 2.5\text{ m/s}$
4. **Position Update:** $\mathbf{x}(t + \Delta t) = \mathbf{x}(t) + \mathbf{v}(t + \Delta t) \cdot \Delta t$
5. **Boundary Containment:** Positions are clamped to $[r, W-r] \times [r, H-r]$ with normal velocity reflection.
