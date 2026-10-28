# CrowdSim — Makefile
# Build, run, test, and clean the particle movement simulator.

CC      = gcc
CFLAGS  = -std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic
LDFLAGS = -fopenmp -lm
TARGET  = crowd_sim
SRC     = crowd_sim.c

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

# ── Automated test suite ─────────────────────────────────────────────
# Each test uses a small workload so the full suite finishes in seconds.
test: $(TARGET)
	@echo "========================================"
	@echo " CrowdSim — Automated Test Suite"
	@echo "========================================"
	@pass=0; fail=0; total=0; \
	\
	run_test () { \
		total=$$((total + 1)); \
		desc="$$1"; shift; \
		if eval "$$@" > /dev/null 2>&1; then \
			echo "  [PASS] $$desc"; \
			pass=$$((pass + 1)); \
		else \
			echo "  [FAIL] $$desc"; \
			fail=$$((fail + 1)); \
		fi; \
	}; \
	\
	run_test "Default execution" \
		"./$(TARGET) --particles 1000 --steps 50 --threads 2"; \
	\
	run_test "Small workload (10 particles, 10 steps)" \
		"./$(TARGET) --particles 10 --steps 10 --threads 1"; \
	\
	run_test "1 thread matches sequential" \
		"./$(TARGET) --particles 5000 --steps 100 --threads 1"; \
	\
	run_test "4 threads match sequential" \
		"./$(TARGET) --particles 5000 --steps 100 --threads 4"; \
	\
	run_test "8 threads match sequential" \
		"./$(TARGET) --particles 5000 --steps 100 --threads 8"; \
	\
	run_test "Boundary validation passes" \
		"./$(TARGET) --particles 2000 --steps 200 --threads 2"; \
	\
	run_test "Reject --particles 0" \
		"! ./$(TARGET) --particles 0"; \
	\
	run_test "Reject --particles -5" \
		"! ./$(TARGET) --particles -5"; \
	\
	run_test "Reject --particles abc" \
		"! ./$(TARGET) --particles abc"; \
	\
	run_test "Reject --steps 0" \
		"! ./$(TARGET) --steps 0"; \
	\
	run_test "Reject --steps -1" \
		"! ./$(TARGET) --steps -1"; \
	\
	run_test "Reject --threads 0" \
		"! ./$(TARGET) --threads 0"; \
	\
	run_test "Reject --threads -2" \
		"! ./$(TARGET) --threads -2"; \
	\
	run_test "Reject unknown option" \
		"! ./$(TARGET) --bogus"; \
	\
	run_test "--help shows usage" \
		"./$(TARGET) --help | grep -q 'Usage'"; \
	\
	echo "========================================"; \
	echo " Results: $$pass passed, $$fail failed ($$total total)"; \
	echo "========================================"; \
	test $$fail -eq 0

clean:
	rm -f $(TARGET)
