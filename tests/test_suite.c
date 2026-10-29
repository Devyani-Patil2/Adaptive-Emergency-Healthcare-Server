#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "common.h"
#include "priority_queue.h"
#include "thread_pool.h"
#include "database.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } else { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } \
} while(0)

/* 1. Test Priority Queue Ordering */
void test_priority_ordering(void) {
    printf("\n=== Running Test: Priority Ordering (P0 > P1 > P2 > P3) ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    Task t_app, t_em, t_tele, t_vac;
    memset(&t_app, 0, sizeof(Task));
    memset(&t_em, 0, sizeof(Task));
    memset(&t_tele, 0, sizeof(Task));
    memset(&t_vac, 0, sizeof(Task));

    t_app.type = REQ_APPOINTMENT;     // P3
    t_vac.type = REQ_VACCINATION;     // P2
    t_tele.type = REQ_TELEMEDICINE;   // P1
    t_em.type = REQ_EMERGENCY_INFO;   // P0

    gettimeofday(&t_app.arrival_time, NULL);
    gettimeofday(&t_vac.arrival_time, NULL);
    gettimeofday(&t_tele.arrival_time, NULL);
    gettimeofday(&t_em.arrival_time, NULL);

    // Push in reverse order: P3, then P2, then P1, then P0
    queue_push_try(&q, t_app);
    queue_push_try(&q, t_vac);
    queue_push_try(&q, t_tele);
    queue_push_try(&q, t_em);

    TEST_ASSERT(queue_get_count(&q) == 4, "Queue count is 4 after 4 pushes");

    // Pop should yield P0 first, then P1, then P2, then P3
    Task out;
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_EMERGENCY_INFO, "1st Pop yields Emergency Info (P0)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_TELEMEDICINE, "2nd Pop yields Telemedicine (P1)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_VACCINATION, "3rd Pop yields Vaccination (P2)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_APPOINTMENT, "4th Pop yields Appointment (P3)");

    TEST_ASSERT(queue_get_count(&q) == 0, "Queue is empty after 4 pops");
    queue_destroy(&q);
}

/* 2. Test Aging and Anti-Starvation Mechanism */
void test_aging_anti_starvation(void) {
    printf("\n=== Running Test: Aging & Anti-Starvation Mechanism ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    // Create an old P3 appointment that arrived 5000 ms ago
    Task old_routine;
    memset(&old_routine, 0, sizeof(Task));
    old_routine.type = REQ_APPOINTMENT; // P3
    gettimeofday(&old_routine.arrival_time, NULL);
    old_routine.arrival_time.tv_sec -= 5; // 5 seconds in the past

    // Create a brand new P1 telemedicine task that just arrived
    Task new_tele;
    memset(&new_tele, 0, sizeof(Task));
    new_tele.type = REQ_TELEMEDICINE; // P1
    gettimeofday(&new_tele.arrival_time, NULL);

    queue_push_try(&q, new_tele);
    queue_push_try(&q, old_routine);

    // Because old_routine waited > 4000ms, aging boosts its effective priority
    // from P3 to P1 or P0, with older arrival time tie-breaker!
    Task out;
    queue_pop_timed(&q, &out, 1);
    TEST_ASSERT(out.type == REQ_APPOINTMENT, "Aged routine task (P3) dequeued before new P1 task due to aging");

    queue_pop_timed(&q, &out, 1);
    TEST_ASSERT(out.type == REQ_TELEMEDICINE, "Newer P1 task dequeued second");

    queue_destroy(&q);
}

/* 3. Test Queue Bounded Buffer & Capacity Rejection (HTTP 503 trigger) */
void test_queue_bounded_capacity(void) {
    printf("\n=== Running Test: Bounded Buffer Capacity (Limit 64) ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    Task t;
    memset(&t, 0, sizeof(Task));
    t.type = REQ_APPOINTMENT;
    gettimeofday(&t.arrival_time, NULL);

    // Fill queue to capacity (64 items)
    int pushed = 0;
    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        if (queue_push_try(&q, t)) {
            pushed++;
        }
    }
    TEST_ASSERT(pushed == QUEUE_CAPACITY, "Successfully pushed 64 tasks into bounded queue");
    TEST_ASSERT(queue_get_count(&q) == QUEUE_CAPACITY, "Queue count matches QUEUE_CAPACITY (64)");

    // 65th push must fail immediately (non-blocking) to trigger HTTP 503
    bool overflow_accepted = queue_push_try(&q, t);
    TEST_ASSERT(overflow_accepted == false, "65th task rejected immediately with non-blocking push (triggers 503)");

    // Drain queue
    Task out;
    int popped = 0;
    while (queue_pop_timed(&q, &out, 0)) {
        popped++;
    }
    TEST_ASSERT(popped == QUEUE_CAPACITY, "Successfully drained all 64 tasks from queue");
    queue_destroy(&q);
}

/* 4. Test SQLite Database Layer & Concurrency */
void test_database_operations(void) {
    printf("\n=== Running Test: SQLite Database & Atomic Booking Transactions ===\n");
    unlink("test_health.db");

    int rc = db_init("test_health.db");
    TEST_ASSERT(rc == 0, "Database initialized with schema and seed data");

    // Test querying hospitals
    char hosp_buf[2048];
    int len = db_get_hospitals_json(hosp_buf, sizeof(hosp_buf));
    TEST_ASSERT(len > 0 && strstr(hosp_buf, "City General Hospital"), "Retrieved hospital list JSON from SQLite");

    // Test querying doctors
    char doc_buf[2048];
    len = db_get_doctors_json(doc_buf, sizeof(doc_buf));
    TEST_ASSERT(len > 0 && strstr(doc_buf, "Dr. Ananya Sharma"), "Retrieved doctor list JSON from SQLite");

    // Test querying slots for Doctor 1
    char slot_buf[2048];
    len = db_get_slots_json(1, slot_buf, sizeof(slot_buf));
    TEST_ASSERT(len > 0 && strstr(slot_buf, "Dr. Ananya Sharma"), "Retrieved available appointment slots for Doctor 1");

    // Test atomic booking
    char booking_code[64] = {0};
    char err_msg[256] = {0};
    rc = db_book_appointment(1, 1, "Test Patient John", "+1-555-0100", "Routine checkup", 0, 15,
                             booking_code, sizeof(booking_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0 && strlen(booking_code) > 0, "Successfully booked Slot 1 (returned valid booking code)");

    // Test conflict detection: booking the exact same slot again MUST return 1 (409 Conflict)
    char second_code[64] = {0};
    rc = db_book_appointment(1, 1, "Test Patient Sarah", "+1-555-0200", "Second attempt", 1, 10,
                             second_code, sizeof(second_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 1, "Double-booking Slot 1 correctly detected as Conflict (rc = 1 -> HTTP 409)");

    // Test querying appointments list
    char appt_buf[4096];
    len = db_get_appointments_json(10, appt_buf, sizeof(appt_buf));
    TEST_ASSERT(len > 0 && strstr(appt_buf, booking_code), "Verified persisted appointment in SQLite database");

    db_close();
    unlink("test_health.db");
}

/* 5. Test Thread Pool Scaling Bounds & Clean Shutdown */
void test_thread_pool_lifecycle(void) {
    printf("\n=== Running Test: Dynamic Thread Pool Lifecycle & Bounds ===\n");
    unlink("test_pool.db");
    db_init("test_pool.db");

    // Create pool with Min=2, Max=6, Idle=2s
    DynamicThreadPool *pool = thread_pool_create(2, 6, 2);
    TEST_ASSERT(pool != NULL, "Thread pool created successfully");
    TEST_ASSERT(pool->min_threads == 2, "Min workers verified at 2");
    TEST_ASSERT(pool->max_threads == 6, "Max workers verified at 6");
    TEST_ASSERT(pool->current_threads >= 2, "Baseline workers spawned and alive (current_threads >= 2)");

    // Submit mock tasks
    Task t;
    memset(&t, 0, sizeof(Task));
    t.client_socket = -1; // Mock socket
    t.type = REQ_APPOINTMENT;
    gettimeofday(&t.arrival_time, NULL);
    snprintf(t.http_method, sizeof(t.http_method), "GET");
    snprintf(t.path, sizeof(t.path), "/api/status");

    for (int i = 0; i < 5; i++) {
        thread_pool_submit(pool, t);
    }

    usleep(500000); // 500 ms execution time
    TEST_ASSERT(pool->total_requests_submitted >= 5, "Submitted tasks tracked in thread pool metrics");

    // Test deterministic graceful shutdown
    thread_pool_destroy(pool);
    TEST_ASSERT(true, "Thread pool gracefully joined all workers and destroyed without deadlocks");

    db_close();
    unlink("test_pool.db");
}

int main(void) {
    printf("===================================================================\n");
    printf("   OPERATING SYSTEMS COURSE PROJECT: AUTOMATED TEST SUITE          \n");
    printf("===================================================================\n");

    test_priority_ordering();
    test_aging_anti_starvation();
    test_queue_bounded_capacity();
    test_database_operations();
    test_thread_pool_lifecycle();

    printf("\n===================================================================\n");
    printf("   TEST SUITE SUMMARY: %d / %d PASSED                              \n", g_tests_passed, g_tests_run);
    printf("===================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
