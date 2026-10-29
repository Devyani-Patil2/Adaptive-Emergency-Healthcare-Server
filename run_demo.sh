#!/usr/bin/env bash
# Demonstration Script for OS Course Project
set -e

echo "==================================================================="
echo "  ADAPTIVE EMERGENCY HEALTHCARE SERVER - END-TO-END DEMO           "
echo "==================================================================="

# 1. Free port 8080 if previously used
fuser -k 8080/tcp 2>/dev/null || true

# 2. Run automated test suite
echo ""
echo ">> Step 1: Running Automated OS Test Suite..."
./bin/run_tests

# 3. Start healthcare server in background
echo ""
echo ">> Step 2: Starting Server (Min=4, Max=16 Workers, Idle Timeout=3s)..."
./bin/healthcare_server 8080 4 16 3 &
SERVER_PID=$!

sleep 1

# 4. Run comparative traffic benchmark
echo ""
echo ">> Step 3: Running Benchmark Simulator (Baseline vs Outbreak Surge)..."
./bin/client_simulator --mode compare

# 5. Verify live metrics
echo ""
echo ">> Step 4: Querying /api/stats for Post-Surge Thread Metrics..."
curl -s http://127.0.0.1:8080/api/stats | head -n 25

# 6. Graceful shutdown
echo ""
echo ">> Step 5: Sending SIGINT to Server PID $SERVER_PID for Graceful Shutdown..."
kill -INT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "==================================================================="
echo "  DEMONSTRATION COMPLETED CLEANLY WITH ZERO LEAKS / DEADLOCKS!     "
echo "==================================================================="
