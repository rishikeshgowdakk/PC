#!/usr/bin/env python3
"""
CrowdSim — Local HTTP backend server.

Serves the web frontend and provides a JSON API for running the compiled
C/OpenMP particle simulation.  Uses only the Python standard library.

Usage:
    python3 server.py              # start on localhost:8080
    python3 server.py 9000         # start on localhost:9000

Press Ctrl-C to stop.
"""

import http.server
import json
import os
import subprocess
import sys
import threading

# ── configuration ────────────────────────────────────────────────────────
HOST = "127.0.0.1"
DEFAULT_PORT = 8080
CROWD_SIM_BIN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "crowd_sim")

# Parameter limits (must match crowd_sim.c)
MAX_PARTICLES = 5_000_000   # web limit is stricter than the C limit
MAX_STEPS     = 1_000_000
MAX_THREADS   = 64
TIMEOUT_SECS  = 120          # kill simulation after this many seconds

# Serialise simulation runs so two requests cannot interfere
_sim_lock = threading.Lock()


def _validate_params(data):
    """Validate and extract simulation parameters from a parsed JSON dict.
    Returns (particles, steps, threads) or raises ValueError."""
    errors = []
    particles = data.get("particles")
    steps     = data.get("steps")
    threads   = data.get("threads")

    for name, val, lo, hi in [("particles", particles, 1, MAX_PARTICLES),
                               ("steps",     steps,     1, MAX_STEPS),
                               ("threads",   threads,   1, MAX_THREADS)]:
        if val is None:
            errors.append(f"'{name}' is required.")
        elif not isinstance(val, int) or isinstance(val, bool):
            errors.append(f"'{name}' must be an integer.")
        elif val < lo or val > hi:
            errors.append(f"'{name}' must be between {lo} and {hi}.")

    if errors:
        raise ValueError("  ".join(errors))

    return particles, steps, threads


class CrowdSimHandler(http.server.SimpleHTTPRequestHandler):
    """Handle API requests and serve static frontend files."""

    # Serve files from web/ subdirectory for all non-API requests
    def translate_path(self, path):
        # Strip query string and fragment before resolving file
        clean_path = path.split("?", 1)[0].split("#", 1)[0]
        if clean_path.startswith("/api/"):
            return clean_path
        # Serve from web/ subdirectory
        web_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")
        rel = clean_path.lstrip("/")
        if rel == "":
            rel = "index.html"
        return os.path.join(web_dir, rel)

    def _send_json(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path != "/api/simulate":
            self._send_json(404, {"error": "Not found."})
            return

        # ── read request body ────────────────────────────────────────
        content_len = int(self.headers.get("Content-Length", 0))
        if content_len > 4096:
            self._send_json(400, {"error": "Request body too large."})
            return
        raw = self.rfile.read(content_len)
        try:
            data = json.loads(raw)
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._send_json(400, {"error": "Invalid JSON."})
            return

        # ── validate parameters ──────────────────────────────────────
        try:
            particles, steps, threads = _validate_params(data)
        except ValueError as exc:
            self._send_json(400, {"error": str(exc)})
            return

        # ── check binary exists ──────────────────────────────────────
        if not os.path.isfile(CROWD_SIM_BIN):
            self._send_json(500, {
                "error": "Simulation binary not found. Run 'make' first."
            })
            return

        # ── run simulation (serialised) ──────────────────────────────
        acquired = _sim_lock.acquire(blocking=False)
        if not acquired:
            self._send_json(429, {
                "error": "Another simulation is already running. "
                         "Please wait for it to finish."
            })
            return

        try:
            cmd = [
                CROWD_SIM_BIN,
                "--particles", str(particles),
                "--steps",     str(steps),
                "--threads",   str(threads),
                "--json",
            ]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=TIMEOUT_SECS,
            )

            if result.returncode != 0:
                err_msg = result.stderr.strip() or "Simulation failed."
                self._send_json(500, {"error": err_msg})
                return

            # Parse the JSON output from the C program
            try:
                sim_result = json.loads(result.stdout)
            except json.JSONDecodeError:
                self._send_json(500, {
                    "error": "Could not parse simulation output.",
                    "raw": result.stdout[:500],
                })
                return

            self._send_json(200, sim_result)

        except subprocess.TimeoutExpired:
            self._send_json(504, {
                "error": f"Simulation timed out after {TIMEOUT_SECS} seconds."
            })
        except Exception as exc:
            self._send_json(500, {"error": f"Server error: {exc}"})
        finally:
            _sim_lock.release()

    # Suppress default access logging for cleaner output
    def log_message(self, fmt, *args):
        msg = fmt % args
        # Only log non-200 or API requests
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

    # Check that the binary exists
    if not os.path.isfile(CROWD_SIM_BIN):
        print(f"Warning: '{CROWD_SIM_BIN}' not found. Run 'make' first.",
              file=sys.stderr)

    # Check web directory
    web_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")
    if not os.path.isdir(web_dir):
        print(f"Error: web directory '{web_dir}' not found.", file=sys.stderr)
        sys.exit(1)

    server = http.server.HTTPServer((HOST, port), CrowdSimHandler)
    print(f"\n  CrowdSim Web Server")
    print(f"  ────────────────────────────────")
    print(f"  URL:     http://{HOST}:{port}/")
    print(f"  Binary:  {CROWD_SIM_BIN}")
    print(f"  Web dir: {web_dir}")
    print(f"  ────────────────────────────────")
    print(f"  Press Ctrl-C to stop.\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.shutdown()


if __name__ == "__main__":
    main()
