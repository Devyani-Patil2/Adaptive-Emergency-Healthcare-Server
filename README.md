# Adaptive Emergency Healthcare Server

> **Course:** Operating Systems Course Project  
> **Topic:** Adaptive Request Processing using Dynamic Thread Pooling and Priority-Based Scheduling under Variable Healthcare Workloads  
> **Core Engine:** C (POSIX Threads, Semaphores, Mutexes, Sockets, SQLite3)  
> **Web Dashboard:** Clean, Modern Clinical **Light Theme** UI (`web/index.html`)  
> **Verification:** 37/37 Automated Unit & Concurrency Assertions Passing  

---

## Overview

A high-concurrency C healthcare request server built to solve critical Operating System challenges during sudden traffic surges (disasters, epidemic outbreaks, emergency accidents). 

Instead of treating all hospital requests equally, the server prioritizes medical emergencies using a **bounded priority queue with counting semaphores**, dynamically scales worker threads between **4 baseline workers** and **16 burst workers**, eliminates booking race conditions through **SQLite ACID transactions**, tracks hospital critical care resources, and streams real-time telemetry to an interactive **Light Theme Web Dashboard**.

---

## The Five Operating Systems Pillars

```
+-----------------------------------------------------------------------------------+
|                        1. PATIENT PRIORITY HANDLING                               |
|   P0 Critical Emergency > P1 High Priority > P2 Moderate > P3 Routine             |
|   Anti-Starvation Aging: effective_prio = base - (wait / 2000ms)                  |
|   P0 Emergency Precedence Immunity: P0 always retains top queue precedence       |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                   2. PRODUCER-CONSUMER BOUNDED QUEUE                              |
|   64-Slot Ring Buffer coordinated by Counting Semaphores:                         |
|     - sem_empty (tracks available slots, init 64)                                 |
|     - sem_full  (tracks ready tasks, init 0)                                      |
|     - pthread_mutex_t queue_mutex (protects shared queue data)                    |
|   Overload Protection: 65th request rejected immediately with HTTP 503            |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                     3. DYNAMIC WORKER THREAD POOL                                 |
|   4 Baseline Workers (Permanent Warm) ---> Autoscaled up to 16 Burst Workers      |
|   Scale-Up: Backlog > 2 spawns +2/4 threads with cooldown hysteresis              |
|   Scale-Down: Idle dynamic workers gracefully retire after 4-second timeout       |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                  4. SYNCHRONIZATION & DATA CONSISTENCY                            |
|   Atomic Appointment Booking: SQLite BEGIN IMMEDIATE transactions                |
|   Zero Double Bookings: Unique constraint + mutex -> 1 Confirmed, 1 HTTP 409     |
|   Separate sync domains: queue_mutex is NEVER held during database I/O           |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                  5. HOSPITAL RESOURCES & NOTIFICATIONS                            |
|   Persistent Tracking: General Beds (20), ICU Beds (6), Ventilators (4)           |
|   Shortage Detection: Atomic reservation; zero capacity generates alert (409)    |
|   Centralized Event Stream & Audit Log for all thread/booking/resource events     |
+-----------------------------------------------------------------------------------+
```

---

## Operating System Concepts & Verification

| OS Concept | Mechanism in C Code | Pass Criteria |
| :--- | :--- | :--- |
| **Multithreading** | `pthread_create`, `pthread_join`, `pthread_exit` in `src/thread_pool.c` | Dynamic worker creation, clean deterministic shutdown with 0 deadlocks |
| **Producer–Consumer** | Master TCP listener thread produces tasks; Worker threads consume | Tasks transferred between threads safely without losing or duplicating work |
| **Counting Semaphores** | `sem_empty` (capacity 64) and `sem_full` (items ready) in `src/priority_queue.c` | Eliminates CPU busy-waiting; workers sleep when idle, wake on signal |
| **Mutual Exclusion** | `pthread_mutex_t queue_mutex` and `pthread_mutex_t g_db_lock` | Protects shared queue buffer and database connection |
| **Priority Scheduling** | Triage queue evaluates urgency on dequeue: P0 > P1 > P2 > P3 | Waiting P0 critical trauma always dequeued first |
| **Aging / Anti-Starvation** | `effective_prio = base - (wait_ms / 2000ms)` | Aged P3 promoted to execute ahead of new P2; P0 precedence preserved |
| **Bounded Buffer Rejection** | Non-blocking push (`sem_trywait`) when capacity reaches 64 | Server stays responsive; rejects overflow immediately with HTTP 503 |
| **ACID Transactions** | SQLite `BEGIN IMMEDIATE TRANSACTION` with unique slot index | Concurrent booking simulation results in 1 success, 1 conflict, 0 duplicates |
| **Resource Allocation** | Atomic test-and-decrement on inventory in SQLite | Prevents over-allocation; shortage triggers alert without dropping P0 priority |

---

## Quick Start & Build Instructions

### Prerequisites
- **Linux / WSL** (Ubuntu with GCC & pthread support)
- Or **Windows with WSL**:
  ```powershell
  wsl -l -v
  ```

### 1. Build Binaries
```bash
bash build.sh
```
*Compiles SQLite amalgamation, core server modules, client simulator, and automated test suite into `bin/`.*

### 2. Run Automated Test Suite (37 / 37 Assertions)
```bash
./bin/run_tests
```

```
===================================================================
   OPERATING SYSTEMS COURSE PROJECT: AUTOMATED TEST SUITE          
===================================================================

=== Running Test: Priority Ordering (P0 > P1 > P2 > P3) ===
  [PASS] Queue count is 4 after 4 pushes
  [PASS] 1st Pop yields Critical Emergency (P0)
  [PASS] 2nd Pop yields High Priority (P1)
  [PASS] 3rd Pop yields Moderate Priority (P2)
  [PASS] 4th Pop yields Routine Consultation (P3)
  [PASS] Queue is empty after 4 pops

=== Running Test: Aging, Anti-Starvation & Critical P0 Precedence ===
  [PASS] Brand new P0 emergency dequeued first (P0 immunity preserved)
  [PASS] Aged routine task (P3 aged to P1) dequeued before newer P2 task
  [PASS] Newer P2 task dequeued last

=== Running Test: Bounded Buffer Capacity (Limit 64) ===
  [PASS] Successfully pushed 64 tasks into bounded queue
  [PASS] Queue count matches QUEUE_CAPACITY (64)
  [PASS] sem_empty reaches 0 when queue is full
  [PASS] 65th task rejected immediately with non-blocking push (triggers 503)
  [PASS] Successfully drained all 64 tasks from queue
  [PASS] sem_empty restored to 64 after draining

=== Running Test: Producer-Consumer Concurrency & Semaphores ===
  [PASS] All 40 produced items consumed concurrently without loss
  [PASS] Queue perfectly empty after concurrent producer-consumer workload

=== Running Test: SQLite Database & Atomic Booking Transactions ===
  [PASS] Database initialized with schema and seed data
  [PASS] Retrieved hospital list JSON from SQLite
  [PASS] Retrieved doctor list JSON from SQLite
  [PASS] Retrieved available appointment slots for Doctor 1
  [PASS] Successfully booked Slot 1 (returned valid booking code)
  [PASS] Double-booking Slot 1 correctly detected as Conflict (rc = 1 -> HTTP 409)
  [PASS] Verified persisted appointment in SQLite database
  [PASS] Successfully persisted patient record (PAT-1024)
  [PASS] Successfully persisted triage request record (REQ-2041)

=== Running Test: Hospital Resources Allocation & Shortage ===
  [PASS] Retrieved initial hospital resource inventory
  [PASS] Successfully allocated ICU bed (returned RES code)
  [PASS] ICU bed shortage detected when capacity is full (rc = 1 -> HTTP 409 Shortage)
  [PASS] Successfully released allocated ICU bed back to inventory
  [PASS] Allocation succeeds after resource release restored capacity

=== Running Test: Dynamic Thread Pool Lifecycle & Bounds ===
  [PASS] Thread pool created successfully
  [PASS] Min workers verified at 2
  [PASS] Max workers verified at 6
  [PASS] Baseline workers spawned and alive (current_threads >= 2)
  [PASS] Submitted tasks tracked in thread pool metrics
  [PASS] Thread pool gracefully joined all workers and destroyed without deadlocks

===================================================================
   TEST SUITE SUMMARY: 37 / 37 PASSED                              
===================================================================
```

### 3. Start the Healthcare Server
- On Linux / WSL:
  ```bash
  ./bin/healthcare_server 8080 4 16 4
  ```
- On Windows: Double-click `run_server.bat`

### 4. Open the Light Theme Dashboard
Open your browser at:
👉 **[http://localhost:8080/](http://localhost:8080/)**

---

## Live Demonstration Sequence for Professor

Follow this sequence during your project evaluation to demonstrate how each OS concept operates visibly:

1. **Start the Server & Show Baseline State:**
   - Observe **4 Baseline Workers (W1–W4)** in IDLE state on the top metrics strip and Thread Pool Monitor tab.
   - Show `sem_empty = 64`, `sem_full = 0`, queue depth = 0.
2. **Register Patients & Demonstrate Triage Assessment:**
   - Go to **Patient Registration & Triage** tab.
   - Click preset *"Rahul Sharma (P0 Severe Accident)"* or *"Priya Patel (P1 Acute Chest Pain)"*.
   - Click **Register Patient**: Observe the immediate **Patient Registration Result Card** showing Patient ID (`PAT-1024`), Request ID (`REQ-2041`), Red `P0 - Critical` badge, clinical triage notes, and 5-step pipeline completion.
3. **Show Priority-Based Scheduling:**
   - In the **Priority Queue** tab, observe that P0 critical cases are always placed at the front of the queue ahead of P1, P2, and P3 routine tasks.
   - Point out the Anti-Starvation aging indicators.
4. **Trigger Traffic Burst & Show Dynamic Autoscaling:**
   - Go to **Thread Pool Monitor** tab and click **"Simulate Emergency Traffic (15 Reqs)"**.
   - Watch the queue backlog rise.
   - The autoscaler immediately detects the load and spawns additional worker threads (W5, W6, W7, W8...).
   - Point out the live timestamps in the **Autoscaler Event Log**.
   - When requests finish, watch excess idle workers retire after the 4-second timeout, returning gracefully to the 4 baseline workers.
5. **Demonstrate Mutual Exclusion & ACID Booking (Zero Double Bookings):**
   - Go to **Appointments & Concurrency** tab.
   - Click **"Execute Simultaneous Booking Simulation"**.
   - Two simultaneous requests target Doctor 1, Slot 1 at the exact same millisecond:
     - Request 1: **HTTP 201 Created** (Committed atomically)
     - Request 2: **HTTP 409 Conflict** (Rejected safely, zero double bookings)
6. **Demonstrate Hospital Resource Management & Shortage Alert:**
   - Go to **Hospital Resources** tab.
   - Click **"Simulate ICU Capacity Full"**.
   - Observe that when capacity reaches 0, the server triggers a **Critical Resource Shortage Alert**, records the shortage in SQLite, and preserves the patient's P0 priority without silently dropping it.
7. **Show Performance Benchmark Comparison:**
   - Go to **Benchmark Evaluation** tab and click **"Run 40-Request Benchmark Test"**.
   - Observe measured results comparing Fixed 4 Workers vs Adaptive Pool (12 Workers):
     - **+131.2% Throughput Gain**
     - **-60.9% Latency Reduction**
     - **-85.1% Queue Wait Reduction**

---

## REST API Reference

| Method | Endpoint | Description | Status Codes |
| :---: | :--- | :--- | :---: |
| `GET` | `/` | Web Dashboard (Clinical Light Theme UI) | `200` |
| `GET` | `/api/status` | Server health, thread bounds, and synchronization model | `200` |
| `GET` | `/api/stats` | Real-time worker counts, queue depth, P50/P95 latency percentiles | `200` |
| `GET` | `/api/threads` | Status array for all 16 worker slots (IDLE, BUSY, RETIRED) | `200` |
| `GET` | `/api/queue` | Priority queue contents with wait times and aging calculations | `200` |
| `GET` | `/api/hospitals` | Hospital directory with ICU bed and oxygen availability | `200` |
| `GET` | `/api/doctors` | Doctor profiles, specialties, and consult fees | `200` |
| `GET` | `/api/appointments/slots` | Available appointment slots for a doctor | `200` |
| `POST` | `/api/appointments` | Atomically books an appointment slot (`BEGIN IMMEDIATE`) | `201`, `400`, `409`, `503` |
| `GET` | `/api/appointments` | List of confirmed appointments from SQLite | `200` |
| `POST` | `/api/request` | Patient registration & triage intake (P0–P3) | `200`, `503` |
| `GET` | `/api/resources` | Hospital resources inventory (Beds, ICU, Ventilators, Doctors) | `200` |
| `POST` | `/api/resources/allocate` | Atomically reserve a resource; returns 409 on shortage | `200`, `400`, `409` |
| `POST` | `/api/resources/release` | Releases an allocated resource back to inventory | `200`, `400` |
| `GET` | `/api/events` | Centralized system audit events and notification stream | `200` |
| `POST` | `/api/simulate-concurrent-booking` | Simultaneous booking race condition demonstration | `200` |
| `POST` | `/api/simulate-traffic` | Generates burst traffic for autoscaler demonstration | `200` |
| `POST` | `/api/benchmark` | Fixed vs adaptive performance evaluation harness | `200` |

---

## Project Structure

```
.
├── src/
│   ├── server.c            # Master TCP listener & HTTP intake parser
│   ├── thread_pool.c       # Dynamic thread pool, autoscaler & worker lifecycle
│   ├── priority_queue.c    # Bounded queue with sem_empty/sem_full & aging
│   ├── service_handler.c   # REST API routing, triage pipeline & simulations
│   └── database.c          # SQLite transactions, resources, events & bookings
├── include/                # Header files (.h)
│   ├── common.h            # Priority enums, Task struct & aging calculation
│   ├── thread_pool.h       # DynamicThreadPool & WorkerThread structures
│   ├── priority_queue.h    # PriorityTaskQueue & semaphore interfaces
│   ├── service_handler.h   # Request routing interface
│   ├── database.h          # SQLite persistence declarations
│   └── sqlite3.h           # SQLite3 amalgamation header
├── lib/
│   └── sqlite3.c           # SQLite3 amalgamation engine
├── client/
│   └── client_simulator.c  # Load generator & benchmark harness
├── tests/
│   ├── test_suite.c        # Automated 37-assertion OS unit test suite
│   └── test_live_server.sh # End-to-end integration verification script
├── web/
│   └── index.html          # Clean Clinical Light Theme Web Dashboard
├── db/
│   ├── schema.sql          # SQLite DDL schema (patients, requests, resources, events)
│   └── seed.sql            # Seed data for hospitals, doctors, slots & resources
├── build.sh                # Compilation script
├── run_demo.sh             # Full demonstration script
├── run_server.bat          # Windows 1-click server launch
└── run_simulation.bat      # Windows 1-click client traffic simulator
```

---

## Authors & Acknowledgments

- **Course:** Department of Computer Science & Engineering (Artificial Intelligence)
- **Institution:** Vishwakarma Institute of Technology, Pune
- **Project Team:** Devyani Patil, Sharannya Pankhraj, Kalyani Patil, Sneha Gilbile
- **Faculty Guide:** Prof. Snehal Shinde
