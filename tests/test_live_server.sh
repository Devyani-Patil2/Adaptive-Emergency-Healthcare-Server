#!/usr/bin/env bash
set -e

echo "=== 1. Checking GET /api/status ==="
curl -s -i http://127.0.0.1:8080/api/status
echo ""

echo "=== 2. Checking GET /api/hospitals ==="
curl -s http://127.0.0.1:8080/api/hospitals
echo ""

echo "=== 3. Checking GET /api/doctors ==="
curl -s http://127.0.0.1:8080/api/doctors
echo ""

echo "=== 4. Checking GET /api/appointments/slots?doctor_id=1 ==="
curl -s "http://127.0.0.1:8080/api/appointments/slots?doctor_id=1"
echo ""

echo "=== 5. First Booking: Slot 1 (Expect 201 Created) ==="
curl -s -i -X POST \
  -H "Content-Type: application/json" \
  -d '{"doctor_id":1,"slot_id":1,"patient_name":"Alice Smith","phone":"+1-555-0100"}' \
  http://127.0.0.1:8080/api/appointments
echo ""

echo "=== 6. Second Booking: Duplicate Slot 1 (Expect 409 Conflict) ==="
curl -s -i -X POST \
  -H "Content-Type: application/json" \
  -d '{"doctor_id":1,"slot_id":1,"patient_name":"Bob Jones","phone":"+1-555-0200"}' \
  http://127.0.0.1:8080/api/appointments
echo ""

echo "=== 7. Checking GET /api/appointments (Verify Alice Smith in DB) ==="
curl -s http://127.0.0.1:8080/api/appointments
echo ""

echo "=== 8. Checking GET /api/stats (OS Thread Pool Metrics) ==="
curl -s http://127.0.0.1:8080/api/stats
echo ""
