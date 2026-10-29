# Adaptive Emergency Healthcare Server

> **Course:** Operating Systems  
> **Language:** C (POSIX Threads, Semaphores, Mutexes, Sockets, SQLite3)  
> **Frontend:** Real-time Web Dashboard (`web/index.html`)

---

## Overview

A high-performance C web server designed for emergency healthcare workloads during sudden traffic surges (outbreaks, vaccination drives, disaster triage). 

It dynamically scales worker threads between **4 baseline workers** and **16 burst workers**, processes requests based on **medical priority with anti-starvation aging**, and persists all records in **SQLite**.

---

## Key Features

- **Dynamic Autoscaling:** Maintains 4 warm baseline threads; automatically spawns up to 16 threads during traffic spikes, and retires idle threads after a 4-second timeout.
- **Producer–Consumer Bounded Queue:** Master thread enqueues incoming TCP connections into a 64-capacity ring buffer using counting semaphores (`sem_empty`, `sem_full`) and a mutex lock.
- **Overload Protection:** Drops excess requests immediately with **HTTP 503** when the queue is full, keeping the server responsive.
- **Priority Triage & Aging:** Requests are scheduled by medical urgency:
  - **P0:** Emergency ICU & Oxygen
  - **P1:** Telemedicine
  - **P2:** Vaccination
  - **P3:** Routine Doctor Appointments  
  *An aging formula promotes waiting P3 requests to prevent starvation.*
- **Database Persistence:** Hospitals, doctors, and appointment slots are stored in SQLite with atomic transactions preventing double-booking (**HTTP 409 Conflict**).
- **Live Web Dashboard:** Real-time visualization of all 16 worker thread states (Active/Idle/Pruned), queue backlog, and latency percentiles (P50/P95).

---

## Operating System Concepts

| OS Concept | Implementation Details |
| :--- | :--- |
| **Multithreading** | POSIX Threads (`pthread_create`, `pthread_join`, `pthread_exit`). |
| **Producer–Consumer** | Master socket thread acts as Producer; Worker threads act as Consumers. |
| **Counting Semaphores** | `sem_empty` (tracks empty slots) and `sem_full` (tracks ready tasks) eliminate busy-waiting. |
| **Mutual Exclusion** | `pthread_mutex_t` protects queue data and shared performance metrics. |
| **Priority Scheduling** | Triage queue evaluates medical urgency on dequeue (P0 > P1 > P2 > P3). |
| **Aging / Anti-Starvation** | Promotes waiting tasks dynamically: `effective_priority = base - (wait_time / 2000ms)`. |
| **TCP Sockets** | Native stream sockets (`socket`, `bind`, `listen`, `accept`, `send`, `recv`). |
| **ACID Transactions** | SQLite `BEGIN IMMEDIATE` transactions prevent double-booking race conditions. |

---

## Quick Start

### 1. Build
```bash
chmod +x build.sh run_demo.sh
./build.sh
```

### 2. Run Automated Tests
```bash
./bin/run_tests
```
*(Runs the 25-assertion unit test suite verifying priority ordering, aging, queue capacity, and SQLite transactions).*

### 3. Start Server
```bash
./bin/healthcare_server 8080 4 16 4
```

### 4. Open Dashboard
Open your browser to:
👉 **[http://localhost:8080/](http://localhost:8080/)**

---

## REST APIs

| Method | Endpoint | Description | Status Codes |
| :---: | :--- | :--- | :---: |
| `GET` | `/` | Web Dashboard | `200` |
| `GET` | `/api/status` | Server health and OS concurrency model | `200` |
| `GET` | `/api/stats` | Real-time thread metrics, queue depth, P50/P95 latency | `200` |
| `GET` | `/api/hospitals` | Hospital list with ICU bed and oxygen availability | `200` |
| `GET` | `/api/doctors` | Doctor profiles, specialties, and fees | `200` |
| `GET` | `/api/appointments/slots?doctor_id=X` | Available appointment slots for a doctor | `200` |
| `POST` | `/api/appointments` | Atomically books an appointment slot | `201`, `400`, `409`, `503` |
| `GET` | `/api/appointments` | List of confirmed appointments | `200` |
| `POST` | `/api/request` | Simulated medical request intake (P0–P3) | `200`, `503` |

---

## Project Structure

```
.
├── src/
│   ├── server.c            # Master TCP listener & HTTP parser
│   ├── thread_pool.c       # Dynamic thread pool & autoscaler
│   ├── priority_queue.c    # Bounded queue with semaphores & aging
│   ├── service_handler.c   # REST API routing & response generation
│   └── database.c          # SQLite transactions & queries
├── include/                # Header files (.h)
├── db/
│   ├── schema.sql          # Relational DDL tables
│   └── seed.sql            # Initial demonstration data
├── lib/
│   └── sqlite3.c           # Vendored SQLite3 amalgamation engine
├── client/
│   └── client_simulator.c  # Load generator & benchmark harness
├── tests/
│   ├── test_suite.c        # Automated 25-assertion C test suite
│   └── test_live_server.sh # Endpoint verification script
├── web/
│   └── index.html          # Real-time web dashboard
├── build.sh                # Compilation script
├── run_demo.sh             # Full automated demonstration script
└── Makefile                # GNU Makefile
```
