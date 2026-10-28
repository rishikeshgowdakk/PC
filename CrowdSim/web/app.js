/* ═══════════════════════════════════════════════════════════════════════
   CrowdSim — Dashboard Application Logic
   ═══════════════════════════════════════════════════════════════════════ */

(function () {
    "use strict";

    /* ── DOM references ────────────────────────────────────────────── */
    const inpParticles = document.getElementById("inp-particles");
    const inpSteps     = document.getElementById("inp-steps");
    const inpThreads   = document.getElementById("inp-threads");
    const btnRun       = document.getElementById("btn-run");
    const btnReset     = document.getElementById("btn-reset");
    const statusMsg    = document.getElementById("status-msg");
    const execLog      = document.getElementById("exec-log");
    const canvas       = document.getElementById("sim-canvas");
    const ctx          = canvas.getContext("2d");

    /* result cells */
    const R = {
        particles: document.getElementById("r-particles"),
        steps:     document.getElementById("r-steps"),
        threads:   document.getElementById("r-threads"),
        seq:       document.getElementById("r-seq"),
        par:       document.getElementById("r-par"),
        speedup:   document.getElementById("r-speedup"),
        eff:       document.getElementById("r-eff"),
        match:     document.getElementById("r-match"),
        bounds:    document.getElementById("r-bounds"),
    };

    /* ── helpers ───────────────────────────────────────────────────── */
    function setStatus(text, cls) {
        statusMsg.className = "status " + (cls || "");
        statusMsg.innerHTML = text;
    }

    function logClear() {
        execLog.innerHTML = "";
    }
    function logLine(text, cls) {
        const p = document.createElement("p");
        p.className = "log-line" + (cls ? " " + cls : "");
        p.textContent = text;
        execLog.appendChild(p);
        execLog.scrollTop = execLog.scrollHeight;
    }

    function clearResults() {
        for (const k in R) {
            R[k].textContent = "—";
            R[k].className = "";
        }
    }

    function validate() {
        const p = parseInt(inpParticles.value, 10);
        const s = parseInt(inpSteps.value, 10);
        const t = parseInt(inpThreads.value, 10);
        const errors = [];
        if (isNaN(p) || p < 1 || p > 5000000)
            errors.push("Particles must be between 1 and 5,000,000.");
        if (isNaN(s) || s < 1 || s > 1000000)
            errors.push("Steps must be between 1 and 1,000,000.");
        if (isNaN(t) || t < 1 || t > 64)
            errors.push("Threads must be between 1 and 64.");
        return errors;
    }

    /* ── canvas drawing ────────────────────────────────────────────── */
    function drawEmpty() {
        const w = canvas.width, h = canvas.height;
        ctx.fillStyle = "#0d1018";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#2a2d3a";
        ctx.lineWidth = 2;
        ctx.strokeRect(1, 1, w - 2, h - 2);
        ctx.fillStyle = "#3a3d4a";
        ctx.font = "14px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText("Run a simulation to see the particle distribution", w / 2, h / 2);
    }

    function drawDensity(grid, gridRows, gridCols) {
        const w = canvas.width, h = canvas.height;

        /* find max density for colour scaling */
        let maxVal = 1;
        for (let r = 0; r < gridRows; r++)
            for (let c = 0; c < gridCols; c++)
                if (grid[r][c] > maxVal) maxVal = grid[r][c];

        const cellW = w / gridCols;
        const cellH = h / gridRows;

        ctx.fillStyle = "#0d1018";
        ctx.fillRect(0, 0, w, h);

        for (let r = 0; r < gridRows; r++) {
            for (let c = 0; c < gridCols; c++) {
                const v = grid[r][c];
                if (v === 0) continue;
                const t = Math.min(v / maxVal, 1.0);
                ctx.fillStyle = densityColor(t);
                ctx.fillRect(c * cellW, r * cellH, cellW + 0.5, cellH + 0.5);
            }
        }

        /* border */
        ctx.strokeStyle = "#4a4d5a";
        ctx.lineWidth = 2;
        ctx.strokeRect(1, 1, w - 2, h - 2);
    }

    function densityColor(t) {
        /* 0 → dark blue,  0.5 → cyan,  0.8 → light,  1.0 → yellow */
        let r, g, b;
        if (t < 0.25) {
            const s = t / 0.25;
            r = Math.round(20 + s * 20);
            g = Math.round(40 + s * 70);
            b = Math.round(80 + s * 110);
        } else if (t < 0.5) {
            const s = (t - 0.25) / 0.25;
            r = Math.round(40 + s * 10);
            g = Math.round(110 + s * 80);
            b = Math.round(190 + s * 55);
        } else if (t < 0.75) {
            const s = (t - 0.5) / 0.25;
            r = Math.round(50 + s * 60);
            g = Math.round(190 + s * 15);
            b = Math.round(245 - s * 50);
        } else {
            const s = (t - 0.75) / 0.25;
            r = Math.round(110 + s * 145);
            g = Math.round(205 + s * 20);
            b = Math.round(195 - s * 130);
        }
        return `rgb(${r},${g},${b})`;
    }

    /* ── run simulation ────────────────────────────────────────────── */
    async function runSimulation() {
        /* validate */
        const errs = validate();
        if (errs.length > 0) {
            setStatus(errs.join(" "), "error");
            return;
        }

        const params = {
            particles: parseInt(inpParticles.value, 10),
            steps:     parseInt(inpSteps.value, 10),
            threads:   parseInt(inpThreads.value, 10),
        };

        /* UI state: running */
        btnRun.disabled = true;
        clearResults();
        logClear();
        drawEmpty();
        setStatus('<span class="spinner"></span> Running simulation…', "info");
        logLine("→ Sending request to backend…", "log-info");
        logLine(`  Particles: ${params.particles}  Steps: ${params.steps}  Threads: ${params.threads}`, "log-info");

        try {
            const resp = await fetch("/api/simulate", {
                method:  "POST",
                headers: { "Content-Type": "application/json" },
                body:    JSON.stringify(params),
            });

            const data = await resp.json();

            if (!resp.ok) {
                setStatus("Error: " + (data.error || "Unknown error"), "error");
                logLine("✗ " + (data.error || "Request failed"), "log-err");
                btnRun.disabled = false;
                return;
            }

            logLine("✓ Simulation completed", "log-ok");

            /* populate results */
            R.particles.textContent = data.particles.toLocaleString();
            R.steps.textContent     = data.steps.toLocaleString();
            R.threads.textContent   = data.threads;
            R.seq.textContent       = data.seq_time_ms.toFixed(3) + " ms";
            R.par.textContent       = data.par_time_ms.toFixed(3) + " ms";
            R.speedup.textContent   = data.speedup.toFixed(2) + "×";
            R.eff.textContent       = data.efficiency.toFixed(1) + "%";

            R.match.textContent = data.match ? "MATCH" : "MISMATCH";
            R.match.className   = data.match ? "val-pass" : "val-fail";
            R.bounds.textContent = data.bounds ? "PASS" : "FAIL";
            R.bounds.className   = data.bounds ? "val-pass" : "val-fail";

            logLine(`  Sequential: ${data.seq_time_ms.toFixed(3)} ms`, "");
            logLine(`  Parallel:   ${data.par_time_ms.toFixed(3)} ms`, "");
            logLine(`  Speedup:    ${data.speedup.toFixed(2)}×`, "");
            logLine(`  Efficiency: ${data.efficiency.toFixed(1)}%`, "");
            logLine(`  Match:      ${data.match ? "MATCH" : "MISMATCH"}`,
                    data.match ? "log-ok" : "log-err");
            logLine(`  Bounds:     ${data.bounds ? "PASS" : "FAIL"}`,
                    data.bounds ? "log-ok" : "log-err");

            /* draw density grid */
            if (data.density && data.grid_rows && data.grid_cols) {
                drawDensity(data.density, data.grid_rows, data.grid_cols);
                logLine("✓ Density grid rendered on canvas", "log-ok");
            }

            if (data.match && data.bounds) {
                setStatus("✓ Simulation complete — all checks passed", "success");
            } else {
                setStatus("⚠ Simulation complete — verification issues detected", "error");
            }

        } catch (err) {
            setStatus("Connection error: " + err.message, "error");
            logLine("✗ " + err.message, "log-err");
        } finally {
            btnRun.disabled = false;
        }
    }

    /* ── reset ─────────────────────────────────────────────────────── */
    function reset() {
        inpParticles.value = "10000";
        inpSteps.value     = "100";
        inpThreads.value   = "4";
        clearResults();
        setStatus("", "");
        logClear();
        execLog.innerHTML = '<p class="log-placeholder">Run a simulation to see the log.</p>';
        drawEmpty();
    }

    /* ── events ────────────────────────────────────────────────────── */
    btnRun.addEventListener("click", runSimulation);
    btnReset.addEventListener("click", reset);

    /* allow Enter key in inputs */
    [inpParticles, inpSteps, inpThreads].forEach(function (inp) {
        inp.addEventListener("keydown", function (e) {
            if (e.key === "Enter") runSimulation();
        });
    });

    /* initial draw */
    drawEmpty();

    /* auto-run support for headless demonstration / screenshots */
    if (new URLSearchParams(window.location.search).get("autorun") === "1") {
        setTimeout(runSimulation, 250);
    }
})();
