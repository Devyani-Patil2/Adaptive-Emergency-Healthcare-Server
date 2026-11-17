# Adaptive Emergency Healthcare Server

> **Course:** Operating Systems Course Project
> **Topic:** Adaptive Request Processing using Dynamic Thread Pooling, Priority-Based Scheduling & Priority-Weighted Load Balancing under Variable Healthcare Workloads
> **Core Engine:** C (POSIX Threads, Semaphores, Mutexes, Sockets, SQLite3)
> **Web Dashboard:** React + Vite — Interactive Clinical Dashboard (Patient Portal + Doctor Command Center)
> **Verification:** 43/43 Automated Unit & Concurrency Assertions Passing

---

## Overview

A high-concurrency C healthcare server built to solve critical Operating System challenges during sudden traffic surges (disasters, epidemic outbreaks, mass casualty incidents).

The server prioritizes medical emergencies using a **bounded priority queue with counting semaphores**, dynamically scales worker threads between **4 baseline** and **16 burst workers**, eliminates booking race conditions through **SQLite ACID transactions**, allocates hospital critical care resources atomically, implements **Priority-Weighted Least-Loaded Core** doctor scheduling, and streams real-time telemetry to a full **React Single Page Application** served directly by the C server.

---

## The Six Operating Systems Pillars

```
+-----------------------------------------------------------------------------------+
|                        1. PATIENT PRIORITY SCHEDULING                             |
|   P0 Critical Emergency > P1 High Priority > P2 Moderate > P3 Routine            |
|   Anti-Starvation Aging: effective_prio = base - (wait_ms / 2000ms)              |
|   P0 Emergency Precedence Immunity: P0 always retains absolute top precedence    |
|   Same-Priority FIFO Tie-Breaking: earlier arrival_time dequeued first           |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                   2. PRODUCER-CONSUMER BOUNDED QUEUE                              |
|   64-Slot Ring Buffer coordinated by Counting Semaphores:                         |
|     - sem_empty  (tracks available slots, initialised to 64)                     |
|     - sem_full   (tracks ready tasks,     initialised to  0)                     |
|     - pthread_mutex_t queue_mutex (protects shared queue data)                   |
|   Overload Protection: 65th concurrent request rejected with HTTP 503            |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                     3. DYNAMIC WORKER THREAD POOL                                 |
|   4 Baseline Workers (Permanent Warm) ──> Autoscaled up to 16 Burst Workers      |
|   Scale-Up  : Backlog > 2 → spawns +2/+4 threads with cooldown hysteresis        |
|   Scale-Down: Idle dynamic workers gracefully retire after 4-second timeout      |
|   Symmetric Multiprocessing (SMP): multiple workers run truly in parallel        |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                  4. SYNCHRONIZATION & DATA CONSISTENCY                            |
|   Atomic Appointment Booking: SQLite BEGIN IMMEDIATE transactions                |
|   Zero Double Bookings: Unique constraint + mutex → 1 Confirmed, 1 HTTP 409     |
|   Separate sync domains: queue_mutex is NEVER held during database I/O           |
|   Appointment & Request status updates protected by g_db_lock (mutex)           |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                  5. HOSPITAL RESOURCES & SHORTAGE ALERTS                          |
|   Persistent Atomic Tracking: ICU Beds (6), General Beds (20),                  |
|     Ventilators (5), Oxygen Cylinders (10), Blood Units (8), Doctors (8)        |
|   Atomic reserve/release with allocation codes (RES-XXXXX)                      |
|   Shortage Detection: zero capacity raises Critical Alert (HTTP 409)            |
|   Full allocation history & audit event log in SQLite                           |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|         6. PRIORITY-WEIGHTED LEAST-LOADED CORE DOCTOR SCHEDULING                 |
|   Problem: Plain round-robin assigned P1 (emergency) patients to already-busy   |
|     doctors while other doctors handled only P3 (routine) patients.              |
|   Fix: Priority-Weighted Load Score per doctor core (worker_id 0-3):            |
|     P0 = weight 8 | P1 = weight 4 | P2 = weight 2 | P3 = weight 1             |
|   New patient always assigned to doctor with LOWEST weighted score               |
|   OS Equivalent: Weighted Fair Queuing (WFQ) + Multi-Core Load Balancing        |
+-----------------------------------------------------------------------------------+
```

---

## Operating System Concepts & Verification

| OS Concept | Mechanism in C Code | Verified By |
| :--- | :--- | :--- |
| **Multithreading** | `pthread_create`, `pthread_join`, `pthread_exit` in `src/thread_pool.c` | 43/43 test assertions; clean shutdown with 0 deadlocks |
| **Producer–Consumer** | Master TCP listener produces tasks; Worker threads consume | Tasks transferred safely without loss or duplication |
| **Counting Semaphores** | `sem_empty` (capacity 64) and `sem_full` (items ready) in `src/priority_queue.c` | Workers sleep when idle, wake on signal — no busy-waiting |
| **Mutual Exclusion** | `pthread_mutex_t queue_mutex` and `pthread_mutex_t g_db_lock` | Protects shared queue buffer and database connection |
| **Priority Scheduling** | Triage queue: P0 > P1 > P2 > P3 on dequeue | Waiting P0 critical case always dequeued first |
| **Aging / Anti-Starvation** | `effective_prio = base - (wait_ms / 2000ms)` | Aged P3 promoted ahead of newer P2; P0 precedence preserved |
| **Bounded Buffer Rejection** | Non-blocking push (`sem_trywait`) at capacity 64 | 65th request rejected immediately with HTTP 503 |
| **FIFO Tie-Breaking (Same Priority)** | `arrival_time` stored per task; earlier timestamp dequeued first on equal priority | PASS: T0 dequeued before T0+2ms for identical P0 tasks |
| **SMP Parallel Execution** | Multiple worker threads process same-priority patients simultaneously on different cores | P0 + P0 → assigned to different doctors, handled in parallel |
| **Priority-Weighted Load Balancing** | `db_get_least_loaded_doctor_id()` computes weighted score per doctor | P1 patient no longer queues behind P0 at busy doctor when P3 doctor is free |
| **ACID Transactions** | SQLite `BEGIN IMMEDIATE` with unique slot index | 1 booking confirmed, 1 HTTP 409, 0 duplicates under concurrency |
| **Resource Allocation** | Atomic test-and-decrement on inventory in SQLite | Prevents over-allocation; shortage alert without dropping P0 priority |

---

## Portals & Features

### 🧑‍⚕️ Patient Portal
- Register acute emergency cases with symptoms, vitals, age, gender, phone
- Choose required resource (ICU Bed, Oxygen, Blood, Ventilator, General Bed)
- System assigns P0–P3 priority based on case type
- **Active Requests** section — shows live status of all ongoing cases (QUEUED → ATTENDED → O2_VENT_DEPLOYED → DISCHARGED)
- **Medical History** section — completed and discharged cases move here automatically; never deleted
- Book specialist outpatient appointments (OPD) with real doctors and time slots
- **Active Appointments** — upcoming confirmed bookings with Cancel option
- **Appointment History** — cancelled/completed bookings preserved in history

### 👨‍⚕️ Doctor Command Center
- **Hospital KPI Cards** — Live counts: Active Emergencies, Patients in Triage, Discharges Today, Resources in Use
- **Multi-Doctor Dispatch Console** — Shows all 4 doctor cores (W1–W4) with real-time busy/idle status and current patient assignment
- **Critical Hospital Resource Inventory** — Live grid: ICU Beds, Oxygen Cylinders, Blood Units, Ventilators, General Beds, Doctors on Duty (available / total)
- **Live Acuity Triage Queue** — All active requests ordered by priority (P0 first); filter by "All Cases" or "My Cases"
- Clinical action buttons per patient: **Attended → Deploy O2 → Discharge**
- **Specialist Outpatient Consultations Board** — View and manage OPD appointments; mark as Attended/Completed
- Each request shows: Patient name, priority badge, symptoms, vitals, assigned doctor core, resource status, current state, timestamps

### 🖥️ Advanced OS Concurrency Diagnostics Console
- Real-time thread pool metrics (workers alive, busy, idle, queue depth)
- Priority Queue inspector with aging calculations
- Autoscaler event log
- Benchmark comparison (Fixed vs Adaptive thread pool)
- Concurrent booking race condition demo
- Same-priority FIFO tie-breaker demo

---

## Same-Priority Handling (OS Concept)

When two patients arrive with **identical priority** (e.g. both P0 Critical):

**Step 1 — SMP Parallel Dispatch:**
Both requests enter the priority queue simultaneously. Since 4 worker threads run in parallel (Symmetric Multiprocessing), both are dequeued and processed concurrently by different workers.

**Step 2 — Priority-Weighted Doctor Assignment:**
Each worker calls `db_get_least_loaded_doctor_id()` which computes:
```
weighted_score[doctor] = Σ (priority_weight × active_patient_count)
  where: P0 = 8, P1 = 4, P2 = 2, P3 = 1
```
The patient is assigned to the doctor with the **lowest weighted score** — ensuring a P1 emergency patient is never queued behind a P0 at a busy doctor when another doctor is only handling a P3 routine case.

**Step 3 — FIFO Tie-Breaking (if single worker available):**
If only one worker is free and two P0 patients arrive simultaneously, the one with the earlier `arrival_time` timestamp is dequeued first (FIFO).

This maps directly to OS concepts: **WFQ (Weighted Fair Queuing)** + **SMP Multi-Core Load Balancing** + **SCHED_FIFO tie-breaking**.

---

## Quick Start & Build Instructions

### Prerequisites
- **Linux / WSL** (Ubuntu with GCC & pthread support)
- **Node.js** (only needed to rebuild the React frontend after code changes)

### 1. Build All Binaries
```bash
bash build.sh
```
Compiles SQLite amalgamation, core server modules, client simulator, and automated test suite into `bin/`.

### 2. Run Automated Test Suite (43 / 43 Assertions)
```bash
./bin/run_tests
```

```
===================================================================
   OPERATING SYSTEMS COURSE PROJECT: AUTOMATED TEST SUITE
===================================================================

=== Running Test: Priority Ordering (P0 > P1 > P2 > P3) ===        [6 PASS]
=== Running Test: Aging, Anti-Starvation & Critical P0 Precedence === [3 PASS]
=== Running Test: Bounded Buffer Capacity (Limit 64) ===            [6 PASS]
=== Running Test: Producer-Consumer Concurrency & Semaphores ===    [2 PASS]
=== Running Test: SQLite Database & Atomic Booking Transactions === [9 PASS]
=== Running Test: Hospital Resources Allocation & Shortage ===      [7 PASS]
=== Running Test: Dynamic Thread Pool Lifecycle & Bounds ===        [6 PASS]
=== Running Test: Same-Priority (P0) FIFO Tie-Breaking ===          [4 PASS]

===================================================================
   TEST SUITE SUMMARY: 43 / 43 PASSED
===================================================================
```

### 3. Start the Healthcare Server

**On Linux / WSL:**
```bash
./bin/healthcare_server 8080 4 16 4
# Args: <port> <min_workers> <max_workers> <idle_timeout_sec>
```

**On Windows — double-click:**
```
run_server.bat
```

### 4. Open the Dashboard

**On Linux/WSL — use the WSL IP:**
```
http://<wsl-ip>:8080/
# Find WSL IP: wsl hostname -I
```

**On Windows (native build or port-forwarding):**
```
http://localhost:8080/
```

### 5. (Optional) Rebuild React Frontend
Only needed if you change JSX/CSS files:
```bash
cd frontend
npm install
npm run build
# Output goes to web/dist/ — served automatically by the C server
```

---

## REST API Reference

| Method | Endpoint | Description | Status Codes |
| :---: | :--- | :--- | :---: |
| `GET` | `/` | React SPA Dashboard (served from `web/dist/`) | `200` |
| `GET` | `/assets/*` | React compiled JS/CSS assets | `200` |
| `GET` | `/api/stats` | Real-time worker counts, queue depth, P50/P95 latency | `200` |
| `GET` | `/api/threads` | Status array for all worker slots (alive, busy, baseline) | `200` |
| `GET` | `/api/queue` | Priority queue contents with wait times & aging | `200` |
| `GET` | `/api/status` | Server health, thread bounds, sync model info | `200` |
| `GET` | `/api/hospitals` | Hospital directory with bed & oxygen availability | `200` |
| `GET` | `/api/doctors` | Doctor profiles, specialties, consultation fees | `200` |
| `GET` | `/api/appointments/slots` | Available OPD slots for a doctor | `200` |
| `GET` | `/api/appointments` | List of confirmed/active appointments from SQLite | `200` |
| `GET` | `/api/resources` | Hospital resource inventory (ICU, O2, Blood, Beds) | `200` |
| `GET` | `/api/requests` | Live triage board — all patient requests with doctor assignment | `200` |
| `GET` | `/api/events` | System audit event log & notification stream | `200` |
| `GET` | `/api/emergency-info` | Emergency contact information | `200` |
| `POST` | `/api/request` | Patient triage registration (P0–P3 priority intake) | `200`, `503` |
| `POST` | `/api/appointments` | Atomically book an OPD appointment slot | `201`, `400`, `409`, `503` |
| `POST` | `/api/appointments/action` | Update appointment status (COMPLETED / CANCELLED) | `200`, `400` |
| `POST` | `/api/requests/action` | Doctor clinical action (ATTENDED / O2_VENT_DEPLOYED) | `200`, `400` |
| `POST` | `/api/requests/discharge` | Discharge patient & release allocated resources | `200`, `400` |
| `POST` | `/api/resources/allocate` | Atomically reserve a resource; 409 on shortage | `200`, `400`, `409` |
| `POST` | `/api/resources/release` | Release an allocated resource back to inventory | `200`, `400` |
| `POST` | `/api/queue/reset` | Clear triage board & fully restore resource inventory | `200` |
| `POST` | `/api/simulate-concurrent-booking` | Race condition booking demonstration | `200` |
| `POST` | `/api/simulate-tie-breaker` | Same-priority FIFO tie-breaking & parallel dispatch demo | `200` |
| `POST` | `/api/simulate-traffic` | Burst traffic load for autoscaler demonstration | `200` |
| `POST` | `/api/benchmark` | Fixed vs Adaptive thread pool performance comparison | `200` |

---

## Project Structure

```
.
├── src/
│   ├── server.c              # Master TCP listener & HTTP parser
│   ├── thread_pool.c         # Dynamic thread pool, autoscaler & worker lifecycle
│   ├── priority_queue.c      # Bounded ring buffer with sem_empty/sem_full & aging
│   ├── service_handler.c     # All REST API routes, triage pipeline & simulations
│   └── database.c            # SQLite ACID transactions, resources, bookings, events
│                               # Includes: db_get_least_loaded_doctor_id()
│                               #   → Priority-Weighted Least-Loaded Core scheduling
├── include/
│   ├── common.h              # Priority enums, Task struct, aging macro
│   ├── thread_pool.h         # DynamicThreadPool & WorkerThread structures
│   ├── priority_queue.h      # PriorityTaskQueue & semaphore interfaces
│   ├── service_handler.h     # Request routing interface
│   ├── database.h            # All SQLite persistence function declarations
│   └── sqlite3.h             # SQLite3 amalgamation header
├── lib/
│   └── sqlite3.c             # SQLite3 amalgamation (single-file database engine)
├── client/
│   └── client_simulator.c    # Load generator & benchmark harness
├── tests/
│   ├── test_suite.c          # 43-assertion OS unit & concurrency test suite
│   └── test_live_server.sh   # End-to-end integration verification script
├── frontend/
│   ├── src/
│   │   ├── App.jsx           # Root app — data fetching, auth, role routing
│   │   └── components/
│   │       ├── PatientPortal.jsx   # Patient portal (triage, appointments, history)
│   │       └── DoctorPortal.jsx    # Doctor command center (triage board, KPIs, OPD)
│   ├── package.json
│   └── vite.config.js
├── web/
│   └── dist/                 # Compiled React SPA — served by C server directly
├── db/
│   ├── schema.sql            # SQLite DDL (patients, requests, resources, events)
│   └── seed.sql              # Seed data: doctors, hospitals, slots, resources
├── build.sh                  # Full compilation script
├── run_demo.sh               # Complete demo sequence script
├── run_server.bat            # Windows 1-click server launch (via WSL)
└── run_simulation.bat        # Windows 1-click client traffic simulator
```

---

## Live Demo Sequence for Evaluation

### 1. Start Server & Show Baseline State
- Open terminal → run `./bin/healthcare_server 8080 4 16 4`
- Open browser → `http://<wsl-ip>:8080/`
- Observe **4 Baseline Workers (W1–W4)** idle in Doctor Command Center thread console
- `sem_empty = 64`, `sem_full = 0`, queue depth = 0

### 2. Patient Portal — Register Emergency Cases
- Enter patient name → Enter Patient Portal
- Submit a **P0 Critical** case (severe accident, requires ICU bed)
- Submit a **P1 High Priority** case (acute chest pain)
- Watch both appear in **Active Emergency Requests** section in real time

### 3. Doctor Command Center — Triage Board
- Select a doctor → Enter Doctor Command Center
- Observe both patients in **Live Acuity Triage Queue** — P0 listed first
- See **different doctors assigned** to same-priority patients (SMP parallel dispatch)
- Perform clinical actions: Attended → Deploy O2 → Discharge
- After discharge, patient disappears from active section and **appears in Patient's Medical History**

### 4. Same-Priority Scheduling Demo
- Register two P0 patients back-to-back
- Observe in triage board that they go to **different doctor cores** (parallel SMP dispatch)
- Register a P1 patient while a doctor has only a P3 patient
- Observe that P1 is assigned to the doctor handling P3 (lower weighted load) — not to a busy P0 doctor

### 5. Dynamic Thread Autoscaling
- Open Advanced Console → Thread Pool Monitor
- Click **"Simulate Emergency Traffic"**
- Watch worker count scale from 4 → 8 → 12 during burst
- After requests complete, idle workers retire back to 4 baseline

### 6. Atomic Booking — Zero Double Bookings
- Advanced Console → Appointments & Concurrency
- Click **"Execute Simultaneous Booking Simulation"**
- Result: 1 × HTTP 201 Confirmed, 1 × HTTP 409 Conflict, 0 duplicates

### 7. Resource Shortage Alert
- Advanced Console → Hospital Resources
- Simulate full ICU capacity
- System raises Critical Shortage Alert and preserves patient's P0 priority

### 8. Run Automated Test Suite
```bash
./bin/run_tests
# Expected: 43 / 43 PASSED — including FIFO tie-breaker test
```

---

## Authors & Acknowledgments

- **Course:** Department of Computer Science & Engineering (Artificial Intelligence)
- **Institution:** Vishwakarma Institute of Technology, Pune
- **Project Team:** Devyani Patil, Sharannya Pankhraj, Kalyani Patil, Sneha Gilbile
- **Faculty Guide:** Prof. Snehal Shinde
