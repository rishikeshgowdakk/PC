#!/usr/bin/env python3
"""
CrowdFlow — Local HTTP Backend Server & API Controller.

Serves the web dashboard and provides a JSON API for running the compiled
C/OpenMP crowd dynamics simulation. Uses only the Python standard library.

Usage:
    python3 server.py              # start on localhost:8080
    python3 server.py 9000         # start on localhost:9000
"""

import http.server
import json
import os
import subprocess
import sys
import threading
import time

HOST = "127.0.0.1"
DEFAULT_PORT = 8080

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CROWDFLOW_BIN = os.path.join(BASE_DIR, "crowdflow")
CROWDSIM_BIN = os.path.join(BASE_DIR, "crowd_sim")
WEB_DIR = os.path.join(BASE_DIR, "web")

# Safe limits for web requests
MAX_AGENTS   = 1_000_000
MAX_STEPS    = 500_000
MAX_THREADS  = 64
TIMEOUT_SECS = 120

_sim_lock = threading.Lock()


def _validate_params(data):
    errors = []
    agents = data.get("agents", data.get("particles", 5000))
    steps = data.get("steps", 50)
    threads = data.get("threads", 4)
    neighbors = str(data.get("neighbors", "grid")).lower()
    schedule = str(data.get("schedule", "static")).lower()
    scenario = str(data.get("scenario", "pillars")).lower()
    compare = bool(data.get("compare", True))
    seed = int(data.get("seed", 42))

    if not isinstance(agents, int) or agents < 1 or agents > MAX_AGENTS:
        errors.append(f"'agents' must be between 1 and {MAX_AGENTS}.")
    if not isinstance(steps, int) or steps < 1 or steps > MAX_STEPS:
        errors.append(f"'steps' must be between 1 and {MAX_STEPS}.")
    if not isinstance(threads, int) or threads < 1 or threads > MAX_THREADS:
        errors.append(f"'threads' must be between 1 and {MAX_THREADS}.")

    if neighbors not in ("grid", "brute"):
        errors.append("'neighbors' must be 'grid' or 'brute'.")
    if schedule not in ("static", "dynamic", "guided"):
        errors.append("'schedule' must be 'static', 'dynamic', or 'guided'.")
    if scenario not in ("empty", "pillars", "bottleneck"):
        errors.append("'scenario' must be 'empty', 'pillars', or 'bottleneck'.")

    if errors:
        raise ValueError(" ".join(errors))

    return {
        "agents": agents,
        "steps": steps,
        "threads": threads,
        "neighbors": neighbors,
        "schedule": schedule,
        "scenario": scenario,
        "compare": compare,
        "seed": seed
    }


class CrowdFlowHandler(http.server.SimpleHTTPRequestHandler):
    def translate_path(self, path):
        clean_path = path.split("?", 1)[0].split("#", 1)[0]
        if clean_path.startswith("/api/"):
            return clean_path
        rel = clean_path.lstrip("/")
        if rel == "":
            rel = "index.html"
        return os.path.join(WEB_DIR, rel)

    def _send_json(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        clean_path = self.path.split("?", 1)[0]
        if clean_path == "/api/system_info":
            import platform
            info = {
                "system": platform.system(),
                "release": platform.release(),
                "machine": platform.machine(),
                "cpu_count": os.cpu_count() or 1,
                "compiler": "GCC 15.2 (C11, -O2, OpenMP)",
                "executable": "crowdflow" if os.path.isfile(CROWDFLOW_BIN) else "not compiled"
            }
            self._send_json(200, info)
            return

        super().do_GET()

    def do_POST(self):
        clean_path = self.path.split("?", 1)[0]

        if clean_path not in ("/api/simulate", "/api/benchmark_scaling"):
            self._send_json(404, {"error": "Endpoint not found."})
            return

        content_len = int(self.headers.get("Content-Length", 0))
        if content_len > 8192:
            self._send_json(400, {"error": "Request body too large."})
            return

        raw = self.rfile.read(content_len)
        try:
            data = json.loads(raw.decode("utf-8")) if raw else {}
        except Exception:
            self._send_json(400, {"error": "Invalid JSON format."})
            return

        try:
            params = _validate_params(data)
        except ValueError as err:
            self._send_json(400, {"error": str(err)})
            return

        bin_path = CROWDFLOW_BIN if os.path.isfile(CROWDFLOW_BIN) else CROWDSIM_BIN
        if not os.path.isfile(bin_path):
            self._send_json(500, {"error": "Simulation binary not found. Run 'make' first."})
            return

        acquired = _sim_lock.acquire(blocking=False)
        if not acquired:
            self._send_json(429, {"error": "Another simulation is currently running. Please wait."})
            return

        try:
            if clean_path == "/api/simulate":
                cmd = [
                    bin_path,
                    "--agents", str(params["agents"]),
                    "--steps", str(params["steps"]),
                    "--threads", str(params["threads"]),
                    "--neighbors", params["neighbors"],
                    "--schedule", params["schedule"],
                    "--scenario", params["scenario"],
                    "--seed", str(params["seed"]),
                    "--json"
                ]
                if params["compare"]:
                    cmd.append("--compare")

                res = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT_SECS)
                if res.returncode != 0:
                    self._send_json(500, {"error": res.stderr.strip() or "Simulation execution failed."})
                    return

                try:
                    sim_result = json.loads(res.stdout)
                except Exception:
                    self._send_json(500, {"error": "Failed to parse simulation output.", "raw": res.stdout[:500]})
                    return

                self._send_json(200, sim_result)

            elif clean_path == "/api/benchmark_scaling":
                # Execute a quick multi-thread scaling sweep
                thread_list = [1, 2, 4, 8]
                scaling_results = []

                # First run sequential baseline
                base_cmd = [
                    bin_path,
                    "--agents", str(params["agents"]),
                    "--steps", str(params["steps"]),
                    "--neighbors", params["neighbors"],
                    "--schedule", params["schedule"],
                    "--scenario", params["scenario"],
                    "--mode", "sequential",
                    "--json"
                ]
                base_res = subprocess.run(base_cmd, capture_output=True, text=True, timeout=TIMEOUT_SECS)
                if base_res.returncode != 0:
                    self._send_json(500, {"error": "Sequential baseline failed."})
                    return
                base_data = json.loads(base_res.stdout)
                seq_time = base_data["time_ms"]

                for t in thread_list:
                    par_cmd = [
                        bin_path,
                        "--agents", str(params["agents"]),
                        "--steps", str(params["steps"]),
                        "--threads", str(t),
                        "--neighbors", params["neighbors"],
                        "--schedule", params["schedule"],
                        "--scenario", params["scenario"],
                        "--mode", "parallel",
                        "--json"
                    ]
                    par_res = subprocess.run(par_cmd, capture_output=True, text=True, timeout=TIMEOUT_SECS)
                    if par_res.returncode == 0:
                        p_data = json.loads(par_res.stdout)
                        p_time = p_data["time_ms"]
                        speedup = seq_time / p_time if p_time > 1e-6 else 0.0
                        eff = (speedup / t) * 100.0
                        scaling_results.append({
                            "threads": t,
                            "time_ms": p_time,
                            "speedup": round(speedup, 2),
                            "efficiency": round(eff, 1)
                        })

                self._send_json(200, {
                    "agents": params["agents"],
                    "steps": params["steps"],
                    "seq_time_ms": seq_time,
                    "scaling": scaling_results
                })

        except subprocess.TimeoutExpired:
            self._send_json(504, {"error": f"Simulation timed out after {TIMEOUT_SECS} seconds."})
        except Exception as exc:
            self._send_json(500, {"error": f"Internal server error: {exc}"})
        finally:
            _sim_lock.release()

    def log_message(self, fmt, *args):
        msg = fmt % args
        if "/api/" in self.path or "200" not in msg:
            sys.stderr.write(f"[server] {self.address_string()} {msg}\n")


def main():
    port = DEFAULT_PORT
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
            if port < 1 or port > 65535:
                raise ValueError
        except ValueError:
            print(f"Usage: {sys.argv[0]} [port]", file=sys.stderr)
            sys.exit(1)

    if not os.path.isfile(CROWDFLOW_BIN):
        print(f"Warning: '{CROWDFLOW_BIN}' not found. Run 'make' first.", file=sys.stderr)

    server = http.server.HTTPServer((HOST, port), CrowdFlowHandler)
    print(f"\n  CrowdFlow Web Dashboard Server")
    print(f"  ──────────────────────────────────────────")
    print(f"  URL:     http://{HOST}:{port}/")
    print(f"  Binary:  {CROWDFLOW_BIN}")
    print(f"  Web dir: {WEB_DIR}")
    print(f"  ──────────────────────────────────────────")
    print(f"  Press Ctrl-C to terminate.\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
        server.shutdown()


if __name__ == "__main__":
    main()
