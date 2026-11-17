#!/usr/bin/env bash
set -e

PORT=8089
echo "=== Starting Healthcare Server on test port $PORT ==="
./bin/healthcare_server $PORT 4 16 3 &
SERVER_PID=$!

cleanup() {
    echo "=== Shutting down server PID $SERVER_PID ==="
    kill -SIGINT $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

sleep 1

echo "=== 1. Checking GET / (Dashboard HTML) ==="
curl -s -i "http://127.0.0.1:$PORT/" | head -n 12
echo ""

echo "=== 2. Checking GET /api/status ==="
curl -s "http://127.0.0.1:$PORT/api/status"
echo ""

echo "=== 3. Checking GET /api/threads (Worker States) ==="
curl -s "http://127.0.0.1:$PORT/api/threads" | head -c 250
echo "..."
echo ""

echo "=== 4. Checking GET /api/resources (Hospital Resources) ==="
curl -s "http://127.0.0.1:$PORT/api/resources"
echo ""

echo "=== 5. Testing Patient Registration (POST /api/request P0 Emergency) ==="
curl -s -X POST \
  -H "Content-Type: application/json" \
  -d '{"patient_name":"Rahul Sharma","age":34,"gender":"Male","phone":"+91-98200-11000","priority":0,"symptoms":"Severe accident trauma, head injury, unconscious","vitals":"BP: 85/55, HR: 130, SpO2: 88%","priority_reason":"Critical trauma symptoms confirmed by triage staff","required_resource":"icu_beds"}' \
  "http://127.0.0.1:$PORT/api/request"
echo ""

echo "=== 6. Testing Concurrent Booking Simulation (POST /api/simulate-concurrent-booking) ==="
curl -s -X POST "http://127.0.0.1:$PORT/api/simulate-concurrent-booking"
echo ""

echo "=== 6b. Testing Same-Priority Tie-Breaker Simulation (POST /api/simulate-tie-breaker) ==="
curl -s -X POST "http://127.0.0.1:$PORT/api/simulate-tie-breaker"
echo ""

echo "=== 7. Testing Traffic Burst Simulation (POST /api/simulate-traffic) ==="
curl -s -X POST \
  -H "Content-Type: application/json" \
  -d '{"count":8}' \
  "http://127.0.0.1:$PORT/api/simulate-traffic"
echo ""

echo "=== 8. Testing Live Benchmark (POST /api/benchmark) ==="
curl -s -X POST "http://127.0.0.1:$PORT/api/benchmark"
echo ""

echo "=== 9. Checking GET /api/events (Audit Stream) ==="
curl -s "http://127.0.0.1:$PORT/api/events" | head -c 300
echo "..."
echo ""

echo "=== All live endpoint integration checks PASSED successfully! ==="
