# CrowdFlow — Makefile
# High-Performance C11 / OpenMP Build, Test, Benchmark, and Web Orchestration

CC       = gcc
CFLAGS   = -std=c11 -O2 -fopenmp -Wall -Wextra -Wpedantic
LDFLAGS  = -fopenmp -lm
PYTHON   = python3

SRC_DIR  = src
OBJ_DIR  = obj
TEST_DIR = tests

SRCS     = $(SRC_DIR)/agent.c \
           $(SRC_DIR)/obstacle.c \
           $(SRC_DIR)/spatial_grid.c \
           $(SRC_DIR)/simulation.c \
           $(SRC_DIR)/benchmark.c \
           $(SRC_DIR)/main.c

OBJS     = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TARGET   = crowdflow

# Legacy target for backward compatibility
LEGACY_TARGET = crowd_sim
LEGACY_SRC    = crowd_sim.c

TEST_TARGET = test_runner
TEST_SRCS   = $(SRC_DIR)/agent.c \
              $(SRC_DIR)/obstacle.c \
              $(SRC_DIR)/spatial_grid.c \
              $(SRC_DIR)/simulation.c \
              $(TEST_DIR)/test_crowdflow.c

.PHONY: all clean run test test-sanitizer benchmark web legacy

all: $(TARGET) $(LEGACY_TARGET)

# ── Compile CrowdFlow modular binary ────────────────────────────────
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Compile legacy CrowdSim binary ──────────────────────────────────
$(LEGACY_TARGET): $(LEGACY_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

legacy: $(LEGACY_TARGET)

# ── Run simulation default ──────────────────────────────────────────
run: $(TARGET)
	./$(TARGET) --agents 5000 --steps 50 --threads 4 --compare

# ── Comprehensive Automated Test Suite ──────────────────────────────
test: $(TARGET) $(LEGACY_TARGET)
	@echo "=========================================================="
	@echo " Running CrowdFlow Unit & Integration Test Suite"
	@echo "=========================================================="
	$(CC) $(CFLAGS) $(TEST_SRCS) -o $(TEST_TARGET) $(LDFLAGS)
	./$(TEST_TARGET)
	@rm -f $(TEST_TARGET)
	@echo "=========================================================="
	@echo " Running Regression Tests for Argument Validation"
	@echo "=========================================================="
	@pass=0; fail=0; total=0; \
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
	run_test "CLI --help displays usage" \
		"./$(TARGET) --help | grep -q 'Usage'"; \
	run_test "Reject invalid --agents 0" \
		"! ./$(TARGET) --agents 0"; \
	run_test "Reject negative --agents -5" \
		"! ./$(TARGET) --agents -5"; \
	run_test "Reject non-numeric --agents abc" \
		"! ./$(TARGET) --agents abc"; \
	run_test "Reject invalid --steps 0" \
		"! ./$(TARGET) --steps 0"; \
	run_test "Reject invalid --threads 0" \
		"! ./$(TARGET) --threads 0"; \
	run_test "Reject invalid option --bogus" \
		"! ./$(TARGET) --bogus"; \
	run_test "Brute force execution mode runs cleanly" \
		"./$(TARGET) --agents 500 --steps 20 --neighbors brute --threads 2"; \
	run_test "Spatial grid execution mode runs cleanly" \
		"./$(TARGET) --agents 2000 --steps 50 --neighbors grid --threads 4"; \
	run_test "CSV export mode emits header row" \
		"./$(TARGET) --agents 500 --steps 10 --csv | grep -q 'Candidate_Checks'"; \
	echo "  Results: $$pass passed, $$fail failed ($$total regression tests)"; \
	test $$fail -eq 0

# ── Memory Sanitizers (AddressSanitizer & UndefinedBehaviorSanitizer) ─
test-sanitizer:
	@echo "=========================================================="
	@echo " Running AddressSanitizer & UndefinedBehaviorSanitizer"
	@echo "=========================================================="
	$(CC) $(CFLAGS) -fsanitize=address,undefined $(TEST_SRCS) -o test_sanitizer $(LDFLAGS)
	./test_sanitizer
	@rm -f test_sanitizer
	@echo "  -> Clean: Zero memory corruption or undefined behaviors!"

# ── Benchmarking Suite ──────────────────────────────────────────────
benchmark: $(TARGET)
	./$(TARGET) --benchmark --agents 5000 --steps 50 --threads 8

# ── Web Dashboard Launch ────────────────────────────────────────────
web: $(TARGET)
	@echo "Starting CrowdFlow Web Dashboard on http://127.0.0.1:8080 ..."
	$(PYTHON) server.py

# ── Clean generated files ───────────────────────────────────────────
clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(LEGACY_TARGET) $(TEST_TARGET) test_sanitizer
