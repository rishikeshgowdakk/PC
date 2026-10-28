(function () {
    "use strict";

    /* ── DOM References ────────────────────────────────────────────── */
    const inpAgents     = document.getElementById("inp-agents");
    const inpSteps      = document.getElementById("inp-steps");
    const inpThreads    = document.getElementById("inp-threads");
    const selScenario   = document.getElementById("sel-scenario");
    const selNeighbors  = document.getElementById("sel-neighbors");
    const selSchedule   = document.getElementById("sel-schedule");

    const btnRun        = document.getElementById("btn-run");
    const btnSweep      = document.getElementById("btn-sweep");
    const btnExport     = document.getElementById("btn-export");
    const btnReset      = document.getElementById("btn-reset");

    const btnViewAgents  = document.getElementById("btn-view-agents");
    const btnViewDensity = document.getElementById("btn-view-density");

    const statusMsg     = document.getElementById("status-msg");
    const execLog       = document.getElementById("exec-log");
    const sysBadge      = document.getElementById("sys-badge");

    const canvas        = document.getElementById("sim-canvas");
    const ctx           = canvas.getContext("2d");

    const kpiSpeedup    = document.getElementById("kpi-speedup");
    const kpiEff        = document.getElementById("kpi-eff");
    const rSeq          = document.getElementById("r-seq");
    const rPar          = document.getElementById("r-par");
    const rChecks       = document.getElementById("r-checks");
    const rNeighbors    = document.getElementById("r-neighbors");
    const rMatch        = document.getElementById("r-match");
    const rInvariants   = document.getElementById("r-invariants");

    const historyBody   = document.getElementById("history-body");
    const scalingContainer = document.getElementById("scaling-container");
    const scalingBars   = document.getElementById("scaling-bars");

    /* ── State ─────────────────────────────────────────────────────── */
    let currentData = null;
    let viewMode = "agents"; /* 'agents' or 'heatmap' */
    let sessionHistory = [];

    const WORLD_W = 100.0;
    const WORLD_H = 60.0;

    /* ── Helpers ───────────────────────────────────────────────────── */
    function setStatus(text, cls) {
        statusMsg.className = "status " + (cls || "");
        statusMsg.innerHTML = text;
    }

    function logLine(text, cls) {
        const p = document.createElement("p");
        p.className = "log-line" + (cls ? " " + cls : "");
        p.textContent = text;
        execLog.appendChild(p);
        execLog.scrollTop = execLog.scrollHeight;
    }

    function logClear() {
        execLog.innerHTML = "";
    }

    /* ── Canvas Rendering ──────────────────────────────────────────── */
    function render() {
        const w = canvas.width;
        const h = canvas.height;
        ctx.fillStyle = "#060911";
        ctx.fillRect(0, 0, w, h);

        /* World boundary border */
        ctx.strokeStyle = "#1e293b";
        ctx.lineWidth = 2;
        ctx.strokeRect(1, 1, w - 2, h - 2);

        if (!currentData) {
            ctx.fillStyle = "#334155";
            ctx.font = "14px system-ui";
            ctx.textAlign = "center";
            ctx.fillText("Run a simulation to view crowd dynamics & performance metrics", w / 2, h / 2);
            return;
        }

        const scaleX = w / WORLD_W;
        const scaleY = h / WORLD_H;

        if (viewMode === "heatmap" && currentData.density) {
            renderHeatmap(scaleX, scaleY);
        } else {
            renderAgents(scaleX, scaleY);
        }

        /* Always draw obstacles */
        if (currentData.obstacles) {
            renderObstacles(scaleX, scaleY);
        }
    }

    function renderObstacles(scaleX, scaleY) {
        currentData.obstacles.forEach(function (obs) {
            if (obs.type === "circle") {
                const cx = obs.cx * scaleX;
                const cy = obs.cy * scaleY;
                const r = obs.radius * scaleX;

                ctx.fillStyle = "rgba(239, 68, 68, 0.25)";
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.fill();

                ctx.strokeStyle = "#ef4444";
                ctx.lineWidth = 2;
                ctx.stroke();
            } else if (obs.type === "aabb") {
                const x = obs.min_x * scaleX;
                const y = obs.min_y * scaleY;
                const rw = (obs.max_x - obs.min_x) * scaleX;
                const rh = (obs.max_y - obs.min_y) * scaleY;

                ctx.fillStyle = "rgba(239, 68, 68, 0.35)";
                ctx.fillRect(x, y, rw, rh);
                ctx.strokeStyle = "#ef4444";
                ctx.lineWidth = 2;
                ctx.strokeRect(x, y, rw, rh);
            }
        });
    }

    function renderAgents(scaleX, scaleY) {
        if (!currentData.sample_agents) return;

        currentData.sample_agents.forEach(function (a) {
            const x = a.x * scaleX;
            const y = a.y * scaleY;

            /* Goal destination marker */
            ctx.fillStyle = "#22c55e";
            ctx.beginPath();
            ctx.arc(a.gx * scaleX, a.gy * scaleY, 2, 0, Math.PI * 2);
            ctx.fill();

            /* Velocity heading line */
            ctx.strokeStyle = "rgba(56, 189, 248, 0.4)";
            ctx.lineWidth = 1.2;
            ctx.beginPath();
            ctx.moveTo(x, y);
            ctx.lineTo(x + a.vx * scaleX * 0.8, y + a.vy * scaleY * 0.8);
            ctx.stroke();

            /* Agent body */
            ctx.fillStyle = (a.id % 2 === 0) ? "#38bdf8" : "#818cf8";
            ctx.beginPath();
            ctx.arc(x, y, 3, 0, Math.PI * 2);
            ctx.fill();
        });
    }

    function renderHeatmap(scaleX, scaleY) {
        const grid = currentData.density;
        const rows = currentData.grid_rows;
        const cols = currentData.grid_cols;
        const cellW = canvas.width / cols;
        const cellH = canvas.height / rows;

        let maxVal = 1;
        for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {
                if (grid[r][c] > maxVal) maxVal = grid[r][c];
            }
        }

        for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {
                const v = grid[r][c];
                if (v === 0) continue;
                const t = Math.min(v / maxVal, 1.0);
                ctx.fillStyle = getDensityColor(t);
                ctx.fillRect(c * cellW, r * cellH, cellW + 0.5, cellH + 0.5);
            }
        }
    }

    function getDensityColor(t) {
        let r, g, b;
        if (t < 0.25) {
            const s = t / 0.25;
            r = Math.round(20 + s * 20); g = Math.round(40 + s * 60); b = Math.round(90 + s * 90);
        } else if (t < 0.5) {
            const s = (t - 0.25) / 0.25;
            r = Math.round(40 + s * 16); g = Math.round(100 + s * 89); b = Math.round(180 + s * 68);
        } else if (t < 0.75) {
            const s = (t - 0.5) / 0.25;
            r = Math.round(56 + s * 150); g = Math.round(189 + s * 30); b = Math.round(248 - s * 148);
        } else {
            const s = (t - 0.75) / 0.25;
            r = Math.round(206 + s * 49); g = Math.round(219 - s * 20); b = Math.round(100 - s * 50);
        }
        return `rgb(${r},${g},${b})`;
    }

    /* ── Action Handlers ───────────────────────────────────────────── */
    async function runSimulation() {
        btnRun.disabled = true;
        btnSweep.disabled = true;
        scalingContainer.style.display = "none";
        setStatus('<span class="spinner"></span> Executing simulation…', "info");
        logClear();

        const payload = {
            agents:    parseInt(inpAgents.value, 10),
            steps:     parseInt(inpSteps.value, 10),
            threads:   parseInt(inpThreads.value, 10),
            scenario:  selScenario.value,
            neighbors: selNeighbors.value,
            schedule:  selSchedule.value,
            compare:   true,
            seed:      42
        };

        logLine(`→ Requesting CrowdFlow execution: ${payload.agents} agents, ${payload.steps} steps, ${payload.threads} threads`, "log-info");
        logLine(`  Algorithm: ${payload.neighbors.toUpperCase()} | Sched: ${payload.schedule.toUpperCase()} | Scenario: ${payload.scenario.toUpperCase()}`, "log-info");

        try {
            const resp = await fetch("/api/simulate", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            const data = await resp.json();
            if (!resp.ok) {
                setStatus("Error: " + (data.error || "Simulation failed"), "error");
                logLine("✗ " + (data.error || "Execution failed"), "log-err");
                btnRun.disabled = false;
                btnSweep.disabled = false;
                return;
            }

            currentData = data;
            render();

            /* Populate KPIs & Results */
            kpiSpeedup.textContent = data.speedup.toFixed(2) + "×";
            kpiEff.textContent = data.efficiency.toFixed(1) + "%";
            rSeq.textContent = data.seq_time_ms.toFixed(2) + " ms";
            rPar.textContent = data.par_time_ms.toFixed(2) + " ms";
            rChecks.textContent = data.candidate_checks.toLocaleString();
            rNeighbors.textContent = data.actual_neighbors.toLocaleString();

            rMatch.textContent = data.match ? "MATCH" : "MISMATCH";
            rMatch.className = data.match ? "val-pass" : "val-fail";

            rInvariants.textContent = data.bounds ? "PASS" : "FAIL";
            rInvariants.className = data.bounds ? "val-pass" : "val-fail";

            logLine(`✓ Completed in ${data.par_time_ms.toFixed(2)} ms (Speedup: ${data.speedup.toFixed(2)}×, Efficiency: ${data.efficiency.toFixed(1)}%)`, "log-ok");
            logLine(`  Candidate neighbor checks: ${data.candidate_checks.toLocaleString()}`, "");
            logLine(`  Numerical Match: ${data.match ? "PASSED (<= 1e-6 tolerance)" : "FAILED"}`, data.match ? "log-ok" : "log-err");
            logLine(`  Boundary & Obstacle Invariants: ${data.bounds ? "PASSED (0 violations)" : "FAILED"}`, data.bounds ? "log-ok" : "log-err");

            setStatus("✓ Simulation completed — state equivalence verified", "success");

            /* Append to session history */
            sessionHistory.push({
                timestamp: new Date().toLocaleTimeString(),
                mode: (payload.neighbors === "grid" ? "Grid" : "Brute"),
                agents: payload.agents,
                threads: payload.threads,
                time: data.par_time_ms.toFixed(1) + " ms",
                speedup: data.speedup.toFixed(2) + "×"
            });
            updateHistoryTable();

        } catch (err) {
            setStatus("Connection Error: " + err.message, "error");
            logLine("✗ Network error: " + err.message, "log-err");
        } finally {
            btnRun.disabled = false;
            btnSweep.disabled = false;
        }
    }

    async function runThreadScalingSweep() {
        btnRun.disabled = true;
        btnSweep.disabled = true;
        setStatus('<span class="spinner"></span> Running multi-thread scaling benchmark (1, 2, 4, 8 threads)…', "info");
        logClear();
        logLine("→ Starting Strong Scaling Sweep across thread allocations…", "log-info");

        const payload = {
            agents:    parseInt(inpAgents.value, 10),
            steps:     parseInt(inpSteps.value, 10),
            scenario:  selScenario.value,
            neighbors: selNeighbors.value,
            schedule:  selSchedule.value
        };

        try {
            const resp = await fetch("/api/benchmark_scaling", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            const data = await resp.json();
            if (!resp.ok) {
                setStatus("Error: " + (data.error || "Benchmark failed"), "error");
                btnRun.disabled = false;
                btnSweep.disabled = false;
                return;
            }

            logLine(`✓ Scaling benchmark complete. Sequential baseline: ${data.seq_time_ms.toFixed(2)} ms`, "log-ok");

            /* Render scaling bars */
            scalingBars.innerHTML = "";
            let maxSpeedup = 1.0;
            data.scaling.forEach(s => { if (s.speedup > maxSpeedup) maxSpeedup = s.speedup; });

            data.scaling.forEach(function (s) {
                logLine(`  Threads: ${s.threads} | Time: ${s.time_ms.toFixed(2)} ms | Speedup: ${s.speedup}× | Efficiency: ${s.efficiency}%`, "");

                const row = document.createElement("div");
                row.className = "scaling-row";

                const pct = Math.min((s.speedup / (maxSpeedup * 1.15)) * 100, 100);
                row.innerHTML = `
                    <div class="scaling-label">${s.threads} Thread${s.threads > 1 ? "s" : ""}</div>
                    <div class="scaling-track"><div class="scaling-fill" style="width: ${pct}%"></div></div>
                    <div class="scaling-val">${s.speedup}× (${s.efficiency}%)</div>
                `;
                scalingBars.appendChild(row);
            });

            scalingContainer.style.display = "block";
            setStatus("✓ Multi-thread scaling benchmark completed", "success");

        } catch (err) {
            setStatus("Error: " + err.message, "error");
        } finally {
            btnRun.disabled = false;
            btnSweep.disabled = false;
        }
    }

    function updateHistoryTable() {
        if (sessionHistory.length === 0) {
            historyBody.innerHTML = '<tr><td colspan="5" class="empty-cell">No runs recorded yet.</td></tr>';
            return;
        }

        historyBody.innerHTML = sessionHistory.slice(-8).reverse().map(function (h) {
            return `<tr>
                <td>${h.mode}</td>
                <td>${h.agents.toLocaleString()}</td>
                <td>${h.threads}</td>
                <td>${h.time}</td>
                <td style="color:var(--accent); font-weight:bold;">${h.speedup}</td>
            </tr>`;
        }).join("");
    }

    function exportCSV() {
        if (sessionHistory.length === 0) {
            alert("No simulation runs recorded in current session.");
            return;
        }

        let csv = "Timestamp,Algorithm,Agents,Threads,Time,Speedup\n";
        sessionHistory.forEach(h => {
            csv += `${h.timestamp},${h.mode},${h.agents},${h.threads},${h.time},${h.speedup}\n`;
        });

        const blob = new Blob([csv], { type: "text/csv" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = `crowdflow_benchmark_${Date.now()}.csv`;
        a.click();
        URL.revokeObjectURL(url);
    }

    function resetAll() {
        inpAgents.value = "5000";
        inpSteps.value = "50";
        inpThreads.value = "4";
        selScenario.value = "pillars";
        selNeighbors.value = "grid";
        selSchedule.value = "static";
        currentData = null;
        kpiSpeedup.textContent = "—";
        kpiEff.textContent = "—";
        rSeq.textContent = "—";
        rPar.textContent = "—";
        rChecks.textContent = "—";
        rNeighbors.textContent = "—";
        rMatch.textContent = "—";
        rInvariants.textContent = "—";
        scalingContainer.style.display = "none";
        setStatus("", "");
        logClear();
        logLine("Reset simulation parameters.", "log-info");
        render();
    }

    /* ── Fetch System Info ─────────────────────────────────────────── */
    async function loadSystemInfo() {
        try {
            const resp = await fetch("/api/system_info");
            if (resp.ok) {
                const info = await resp.json();
                sysBadge.textContent = `${info.machine} (${info.cpu_count} cores) • ${info.compiler}`;
            }
        } catch (_) {
            sysBadge.textContent = "Local Native Engine";
        }
    }

    /* ── Event Listeners ───────────────────────────────────────────── */
    btnRun.addEventListener("click", runSimulation);
    btnSweep.addEventListener("click", runThreadScalingSweep);
    btnExport.addEventListener("click", exportCSV);
    btnReset.addEventListener("click", resetAll);

    btnViewAgents.addEventListener("click", function () {
        viewMode = "agents";
        btnViewAgents.classList.add("active");
        btnViewDensity.classList.remove("active");
        render();
    });

    btnViewDensity.addEventListener("click", function () {
        viewMode = "heatmap";
        btnViewDensity.classList.add("active");
        btnViewAgents.classList.remove("active");
        render();
    });

    loadSystemInfo();
    render();

    /* Headless screenshot / auto-run query parameter */
    if (new URLSearchParams(window.location.search).get("autorun") === "1") {
        setTimeout(runSimulation, 300);
    }
})();
