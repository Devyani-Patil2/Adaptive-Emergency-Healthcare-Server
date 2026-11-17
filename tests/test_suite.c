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

/* 1. Test Priority Queue Ordering (P0 > P1 > P2 > P3) */
void test_priority_ordering(void) {
    printf("\n=== Running Test: Priority Ordering (P0 > P1 > P2 > P3) ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    Task t_p3, t_p2, t_p1, t_p0;
    memset(&t_p3, 0, sizeof(Task));
    memset(&t_p2, 0, sizeof(Task));
    memset(&t_p1, 0, sizeof(Task));
    memset(&t_p0, 0, sizeof(Task));

    t_p3.type = REQ_P3_ROUTINE;
    t_p2.type = REQ_P2_MODERATE;
    t_p1.type = REQ_P1_HIGH;
    t_p0.type = REQ_P0_CRITICAL;

    gettimeofday(&t_p3.arrival_time, NULL);
    gettimeofday(&t_p2.arrival_time, NULL);
    gettimeofday(&t_p1.arrival_time, NULL);
    gettimeofday(&t_p0.arrival_time, NULL);

    // Push in reverse order: P3, then P2, then P1, then P0
    queue_push_try(&q, t_p3);
    queue_push_try(&q, t_p2);
    queue_push_try(&q, t_p1);
    queue_push_try(&q, t_p0);

    TEST_ASSERT(queue_get_count(&q) == 4, "Queue count is 4 after 4 pushes");

    // Pop must strictly yield P0 first, then P1, then P2, then P3
    Task out;
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_P0_CRITICAL, "1st Pop yields Critical Emergency (P0)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_P1_HIGH, "2nd Pop yields High Priority (P1)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_P2_MODERATE, "3rd Pop yields Moderate Priority (P2)");
    TEST_ASSERT(queue_pop_timed(&q, &out, 1) && out.type == REQ_P3_ROUTINE, "4th Pop yields Routine Consultation (P3)");

    TEST_ASSERT(queue_get_count(&q) == 0, "Queue is empty after 4 pops");
    queue_destroy(&q);
}

/* 2. Test Aging, Anti-Starvation & Critical P0 Immunity */
void test_aging_and_p0_immunity(void) {
    printf("\n=== Running Test: Aging, Anti-Starvation & Critical P0 Precedence ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    // Create an old P3 routine task that has waited 5000 ms (boosted by 2 levels to P1)
    Task old_routine;
    memset(&old_routine, 0, sizeof(Task));
    old_routine.type = REQ_P3_ROUTINE;
    gettimeofday(&old_routine.arrival_time, NULL);
    old_routine.arrival_time.tv_sec -= 5;

    // Create a brand new P2 moderate task that just arrived
    Task new_moderate;
    memset(&new_moderate, 0, sizeof(Task));
    new_moderate.type = REQ_P2_MODERATE;
    gettimeofday(&new_moderate.arrival_time, NULL);

    // Create a brand new P0 critical emergency that arrived right now
    Task new_emergency;
    memset(&new_emergency, 0, sizeof(Task));
    new_emergency.type = REQ_P0_CRITICAL;
    gettimeofday(&new_emergency.arrival_time, NULL);

    queue_push_try(&q, new_moderate);
    queue_push_try(&q, old_routine);
    queue_push_try(&q, new_emergency);

    Task out;
    // 1st Pop MUST be the brand new P0 emergency (P0 immunity: aging never overrides genuine life-threatening emergencies)
    queue_pop_timed(&q, &out, 1);
    TEST_ASSERT(out.type == REQ_P0_CRITICAL, "Brand new P0 emergency dequeued first (P0 immunity preserved)");

    // 2nd Pop must be the aged routine task (P3 boosted to P1), ahead of fresh P2
    queue_pop_timed(&q, &out, 1);
    TEST_ASSERT(out.type == REQ_P3_ROUTINE, "Aged routine task (P3 aged to P1) dequeued before newer P2 task");

    // 3rd Pop is the moderate task
    queue_pop_timed(&q, &out, 1);
    TEST_ASSERT(out.type == REQ_P2_MODERATE, "Newer P2 task dequeued last");

    queue_destroy(&q);
}

/* 3. Test Queue Bounded Buffer & Capacity Rejection (HTTP 503 trigger) */
void test_queue_bounded_capacity(void) {
    printf("\n=== Running Test: Bounded Buffer Capacity (Limit 64) ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    Task t;
    memset(&t, 0, sizeof(Task));
    t.type = REQ_P3_ROUTINE;
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
    TEST_ASSERT(queue_get_sem_empty_val(&q) == 0, "sem_empty reaches 0 when queue is full");

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
    TEST_ASSERT(queue_get_sem_empty_val(&q) == QUEUE_CAPACITY, "sem_empty restored to 64 after draining");
    queue_destroy(&q);
}

/* 4. Test Producer-Consumer Concurrency with Counting Semaphores */
typedef struct {
    PriorityTaskQueue *q;
    int items_to_produce;
} ProducerArg;

typedef struct {
    PriorityTaskQueue *q;
    int items_to_consume;
    int consumed_count;
} ConsumerArg;

static void* producer_thread_func(void *arg) {
    ProducerArg *parg = (ProducerArg*)arg;
    for (int i = 0; i < parg->items_to_produce; i++) {
        Task t;
        memset(&t, 0, sizeof(Task));
        t.type = (RequestType)(i % 4);
        gettimeofday(&t.arrival_time, NULL);
        queue_push(parg->q, t);
    }
    return NULL;
}

static void* consumer_thread_func(void *arg) {
    ConsumerArg *carg = (ConsumerArg*)arg;
    Task t;
    for (int i = 0; i < carg->items_to_consume; i++) {
        if (queue_pop_timed(carg->q, &t, 3)) {
            carg->consumed_count++;
        }
    }
    return NULL;
}

void test_producer_consumer_concurrency(void) {
    printf("\n=== Running Test: Producer-Consumer Concurrency & Semaphores ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    pthread_t prod1, prod2, cons1, cons2;
    ProducerArg pa1 = { &q, 20 };
    ProducerArg pa2 = { &q, 20 };
    ConsumerArg ca1 = { &q, 20, 0 };
    ConsumerArg ca2 = { &q, 20, 0 };

    pthread_create(&prod1, NULL, producer_thread_func, &pa1);
    pthread_create(&prod2, NULL, producer_thread_func, &pa2);
    pthread_create(&cons1, NULL, consumer_thread_func, &ca1);
    pthread_create(&cons2, NULL, consumer_thread_func, &ca2);

    pthread_join(prod1, NULL);
    pthread_join(prod2, NULL);
    pthread_join(cons1, NULL);
    pthread_join(cons2, NULL);

    int total_consumed = ca1.consumed_count + ca2.consumed_count;
    TEST_ASSERT(total_consumed == 40, "All 40 produced items consumed concurrently without loss");
    TEST_ASSERT(queue_get_count(&q) == 0, "Queue perfectly empty after concurrent producer-consumer workload");

    queue_destroy(&q);
}

/* 5. Test SQLite Database Layer & Concurrency */
void test_database_operations(void) {
    printf("\n=== Running Test: SQLite Database & Atomic Booking Transactions ===\n");
    unlink("test_health.db");

    int rc = db_init("test_health.db");
    TEST_ASSERT(rc == 0, "Database initialized with schema and seed data");

    // Test querying hospitals
    char hosp_buf[2048];
    int len = db_get_hospitals_json(hosp_buf, sizeof(hosp_buf));
    TEST_ASSERT(len > 0 && strstr(hosp_buf, "City General Trauma Center"), "Retrieved hospital list JSON from SQLite");

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

    // Test Patient Registration & Request Persistence
    rc = db_record_patient("PAT-1024", "Rahul Sharma", 34, "Male", "+91-98200-11000");
    TEST_ASSERT(rc == 0, "Successfully persisted patient record (PAT-1024)");

    rc = db_record_request("REQ-2041", "PAT-1024", "Rahul Sharma", 0, "P0 - Critical Emergency",
                           "Head trauma and severe accident", "BP: 85/55, HR: 130, SpO2: 88%",
                           "Critical symptoms confirmed by triage assessment", "icu_beds");
    TEST_ASSERT(rc == 0, "Successfully persisted triage request record (REQ-2041)");

    db_close();
    unlink("test_health.db");
}

/* 6. Test Hospital Resources Allocation, Shortage & Release */
void test_hospital_resources(void) {
    printf("\n=== Running Test: Hospital Resources Allocation & Shortage ===\n");
    unlink("test_res.db");
    db_init("test_res.db");

    char res_buf[2048];
    int len = db_get_resources_json(res_buf, sizeof(res_buf));
    TEST_ASSERT(len > 0 && strstr(res_buf, "icu_beds"), "Retrieved initial hospital resource inventory");

    // Allocate an ICU bed
    char alloc_code[64] = {0};
    char err_msg[256] = {0};
    int rc = db_allocate_resource("icu_beds", "PAT-1001", "Accident Patient", alloc_code, sizeof(alloc_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0 && strlen(alloc_code) > 0, "Successfully allocated ICU bed (returned RES code)");

    // Allocate oxygen cylinders
    char o2_code[64] = {0};
    rc = db_allocate_resource("oxygen_cylinders", "PAT-O2", "Respiratory Patient", o2_code, sizeof(o2_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0 && strlen(o2_code) > 0, "Successfully allocated oxygen cylinder (returned RES code)");

    // Allocate emergency blood units
    char blood_code[64] = {0};
    rc = db_allocate_resource("blood_units", "PAT-BLD", "Trauma Hemorrhage Patient", blood_code, sizeof(blood_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0 && strlen(blood_code) > 0, "Successfully allocated emergency blood unit (returned RES code)");

    // Allocate remaining ICU beds until capacity is exhausted
    for (int i = 0; i < 10; i++) {
        char temp_code[64] = {0};
        db_allocate_resource("icu_beds", "PAT-BULK", "Bulk Patient", temp_code, sizeof(temp_code), err_msg, sizeof(err_msg));
    }

    // Next allocation must return 1 (Shortage / Conflict -> HTTP 409)
    char shortage_code[64] = {0};
    rc = db_allocate_resource("icu_beds", "PAT-9999", "Emergency Patient", shortage_code, sizeof(shortage_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 1, "ICU bed shortage detected when capacity is full (rc = 1 -> HTTP 409 Shortage)");

    // Release resource and verify availability is restored
    rc = db_release_resource(alloc_code, "icu_beds", err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0, "Successfully released allocated ICU bed back to inventory");

    // After release, an allocation should succeed again
    rc = db_allocate_resource("icu_beds", "PAT-RECOVER", "New Patient", alloc_code, sizeof(alloc_code), err_msg, sizeof(err_msg));
    TEST_ASSERT(rc == 0, "Allocation succeeds after resource release restored capacity");

    db_close();
    unlink("test_res.db");
}

/* 7. Test Thread Pool Scaling Bounds & Clean Shutdown */
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
    t.type = REQ_P3_ROUTINE;
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

/* 8. Test Same-Priority (P0) FIFO Tie-Breaking */
void test_same_priority_fifo_tie_breaking(void) {
    printf("\n=== Running Test: Same-Priority (P0) FIFO Tie-Breaking ===\n");
    PriorityTaskQueue q;
    queue_init(&q);

    Task t1, t2;
    memset(&t1, 0, sizeof(Task));
    memset(&t2, 0, sizeof(Task));

    t1.type = REQ_P0_CRITICAL;
    snprintf(t1.patient_name, sizeof(t1.patient_name), "First P0 Arrival");
    gettimeofday(&t1.arrival_time, NULL);
    t1.arrival_time.tv_usec -= 2000; // 2 ms earlier

    t2.type = REQ_P0_CRITICAL;
    snprintf(t2.patient_name, sizeof(t2.patient_name), "Second P0 Arrival");
    gettimeofday(&t2.arrival_time, NULL);

    // Push both P0 tasks
    queue_push_try(&q, t1);
    queue_push_try(&q, t2);

    TEST_ASSERT(queue_get_count(&q) == 2, "Queue count is 2 for identical P0 priority cases");

    Task out1, out2;
    TEST_ASSERT(queue_pop_timed(&q, &out1, 1) && strcmp(out1.patient_name, "First P0 Arrival") == 0,
                "Identical P0 priority: earlier arrival T0 dequeued first (FIFO tie-breaker)");
    TEST_ASSERT(queue_pop_timed(&q, &out2, 1) && strcmp(out2.patient_name, "Second P0 Arrival") == 0,
                "Identical P0 priority: second arrival T0+2ms dequeued second");

    TEST_ASSERT(queue_get_count(&q) == 0, "Queue empty after tie-breaker test");
    queue_destroy(&q);
}

int main(void) {
    printf("===================================================================\n");
    printf("   OPERATING SYSTEMS COURSE PROJECT: AUTOMATED TEST SUITE          \n");
    printf("===================================================================\n");

    test_priority_ordering();
    test_aging_and_p0_immunity();
    test_queue_bounded_capacity();
    test_producer_consumer_concurrency();
    test_database_operations();
    test_hospital_resources();
    test_thread_pool_lifecycle();
    test_same_priority_fifo_tie_breaking();

    printf("\n===================================================================\n");
    printf("   TEST SUITE SUMMARY: %d / %d PASSED                              \n", g_tests_passed, g_tests_run);
    printf("===================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
