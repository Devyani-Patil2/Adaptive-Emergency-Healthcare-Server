#include "thread_pool.h"
#include "service_handler.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* worker_routine(void *arg);
static bool spawn_worker_slot(DynamicThreadPool *pool, int worker_index, bool is_baseline);

static int compare_ints(const void *a, const void *b) {
    return (*(const int*)a - *(const int*)b);
}

DynamicThreadPool* thread_pool_create(int min_threads, int max_threads, int idle_timeout) {
    if (min_threads < MIN_WORKERS_ALLOWED) min_threads = DEFAULT_MIN_THREADS;
    if (max_threads > MAX_WORKERS_LIMIT) max_threads = MAX_WORKERS_LIMIT;
    if (max_threads < min_threads) max_threads = min_threads;
    if (idle_timeout < 1) idle_timeout = DEFAULT_IDLE_TIMEOUT_SEC;

    DynamicThreadPool *pool = (DynamicThreadPool*)malloc(sizeof(DynamicThreadPool));
    if (!pool) return NULL;

    pool->min_threads = min_threads;
    pool->max_threads = max_threads;
    pool->idle_timeout_sec = idle_timeout;
    pool->current_threads = 0;
    pool->busy_threads = 0;
    pool->is_shutdown = false;
    pool->last_scale_time_ms = current_timestamp_ms();

    pool->total_requests_submitted = 0;
    pool->total_requests_succeeded = 0;
    pool->total_requests_failed = 0;
    pool->total_scale_up_events = 0;
    pool->total_scale_down_events = 0;
    pool->total_turnaround_time_ms = 0.0;
    pool->latency_sample_count = 0;
    pool->latency_sample_idx = 0;

    pthread_mutex_init(&pool->pool_lock, NULL);

    // Initialize bounded priority queue with counting semaphores
    queue_init(&pool->queue);

    // Initialize worker slots
    for (int i = 0; i < MAX_WORKERS_LIMIT; i++) {
        pool->workers[i].worker_id = i;
        pool->workers[i].thread_id = 0;
        pool->workers[i].has_thread = false;
        pool->workers[i].is_alive = false;
        pool->workers[i].is_busy = false;
        pool->workers[i].is_baseline = (i < min_threads);
        pool->workers[i].last_busy_time_ms = current_timestamp_ms();
        pool->workers[i].spawn_time_ms = current_timestamp_ms();
        pool->workers[i].requests_processed = 0;
        memset(pool->workers[i].current_task_id, 0, sizeof(pool->workers[i].current_task_id));
        memset(pool->workers[i].current_patient, 0, sizeof(pool->workers[i].current_patient));
        pool->workers[i].current_priority = -1;
        pool->workers[i].pool = pool;
    }

    // Spawn the baseline minimum worker threads
    pthread_mutex_lock(&pool->pool_lock);
    for (int i = 0; i < min_threads; i++) {
        if (spawn_worker_slot(pool, i, true)) {
            pool->current_threads++;
        }
    }
    pthread_mutex_unlock(&pool->pool_lock);

    printf("[THREAD POOL] Initialized: Min Workers=%d, Max Workers=%d, Idle Timeout=%ds, Queue Cap=%d\n",
           pool->min_threads, pool->max_threads, pool->idle_timeout_sec, QUEUE_CAPACITY);

    return pool;
}

static bool spawn_worker_slot(DynamicThreadPool *pool, int worker_index, bool is_baseline) {
    WorkerThread *worker = &pool->workers[worker_index];

    if (worker->is_alive) {
        return false;
    }

    if (worker->has_thread) {
        pthread_join(worker->thread_id, NULL);
        worker->has_thread = false;
        worker->thread_id = 0;
    }

    worker->is_alive = true;
    worker->is_busy = false;
    worker->is_baseline = is_baseline;
    worker->last_busy_time_ms = current_timestamp_ms();
    worker->spawn_time_ms = current_timestamp_ms();
    memset(worker->current_task_id, 0, sizeof(worker->current_task_id));
    memset(worker->current_patient, 0, sizeof(worker->current_patient));
    worker->current_priority = -1;

    if (pthread_create(&worker->thread_id, NULL, worker_routine, worker) != 0) {
        worker->is_alive = false;
        worker->has_thread = false;
        return false;
    }
    worker->has_thread = true;
    return true;
}

static void* worker_routine(void *arg) {
    WorkerThread *worker = (WorkerThread*)arg;
    DynamicThreadPool *pool = worker->pool;

    while (true) {
        Task task;
        bool got_task = false;

        // Baseline workers wait with short poll for shutdown; dynamic workers wait with idle_timeout
        if (worker->is_baseline) {
            got_task = queue_pop_timed(&pool->queue, &task, 2);
        } else {
            got_task = queue_pop_timed(&pool->queue, &task, pool->idle_timeout_sec);
        }

        // Check for shutdown
        pthread_mutex_lock(&pool->pool_lock);
        if (pool->is_shutdown) {
            worker->is_alive = false;
            worker->is_busy = false;
            pool->current_threads--;
            pthread_mutex_unlock(&pool->pool_lock);
            pthread_exit(NULL);
        }

        if (!got_task) {
            // Check if eligible dynamic worker should retire
            long long now_ms = current_timestamp_ms();
            long long idle_ms = now_ms - worker->last_busy_time_ms;

            if (!worker->is_baseline && 
                pool->current_threads > pool->min_threads &&
                idle_ms >= (long long)pool->idle_timeout_sec * 1000) {

                printf("  [AUTOSCALER - SCALE DOWN] Worker W%d idle for %lld ms. Retiring dynamic worker thread. Total workers: %d -> %d\n",
                       worker->worker_id + 1, idle_ms, pool->current_threads, pool->current_threads - 1);

                char event_msg[128];
                snprintf(event_msg, sizeof(event_msg),
                         "Idle worker W%d retired after %ds grace period. Workers: %d -> %d",
                         worker->worker_id + 1, pool->idle_timeout_sec, pool->current_threads, pool->current_threads - 1);
                db_log_event("THREAD_SCALED_DOWN", "INFO", event_msg, "AUTOSCALER");

                worker->is_alive = false;
                worker->is_busy = false;
                pool->current_threads--;
                pool->total_scale_down_events++;
                pthread_mutex_unlock(&pool->pool_lock);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&pool->pool_lock);
            continue;
        }

        // Mark worker as active & update task metadata
        worker->is_busy = true;
        pool->busy_threads++;
        snprintf(worker->current_task_id, sizeof(worker->current_task_id), "%s",
                 strlen(task.request_id) > 0 ? task.request_id : "REQ-ACTIVE");
        snprintf(worker->current_patient, sizeof(worker->current_patient), "%s",
                 strlen(task.patient_name) > 0 ? task.patient_name : "Patient");
        worker->current_priority = (int)task.type;
        worker->requests_processed++;
        pthread_mutex_unlock(&pool->pool_lock);

        // Process healthcare transaction
        process_healthcare_task(&task, worker->worker_id, pool);

        // Mark worker as idle
        pthread_mutex_lock(&pool->pool_lock);
        worker->is_busy = false;
        worker->last_busy_time_ms = current_timestamp_ms();
        memset(worker->current_task_id, 0, sizeof(worker->current_task_id));
        memset(worker->current_patient, 0, sizeof(worker->current_patient));
        worker->current_priority = -1;
        pool->busy_threads--;
        pthread_mutex_unlock(&pool->pool_lock);
    }

    return NULL;
}

/*
 * Enqueues a task and evaluates dynamic scaling policy.
 * Applies cooldown & hysteresis to avoid rapid thread flapping.
 */
bool thread_pool_submit(DynamicThreadPool *pool, Task task) {
    if (!pool || pool->is_shutdown) return false;

    // Push into thread-safe bounded priority queue
    // Uses queue_push_try so if queue is full (64 items), it returns false immediately
    if (!queue_push_try(&pool->queue, task)) {
        pthread_mutex_lock(&pool->pool_lock);
        pool->total_requests_failed++;
        pthread_mutex_unlock(&pool->pool_lock);
        db_log_event("QUEUE_OVERLOAD", "CRITICAL", "Bounded queue full (64/64). Overload protection rejected request with HTTP 503.", "OVERFLOW");
        return false; // Queue full -> triggers HTTP 503 Overload response
    }

    pthread_mutex_lock(&pool->pool_lock);
    pool->total_requests_submitted++;
    int queue_depth = pool->queue.count;
    long long now_ms = current_timestamp_ms();

    // Autoscaling Policy:
    // Scale up if:
    // 1. All currently active workers are busy (or queue depth > active workers).
    // 2. Queue backlog exists (queue_depth >= 1).
    // 3. Pool is below max_threads (16).
    // 4. Cooldown window has elapsed since last scale event (hysteresis).
    if ((pool->busy_threads >= pool->current_threads || queue_depth >= 2) &&
        pool->current_threads < pool->max_threads &&
        queue_depth > 0 &&
        (now_ms - pool->last_scale_time_ms >= SCALE_COOLDOWN_MS)) {

        // Determine step size: scale aggressively during sudden bursts
        int workers_to_add = 2;
        if (queue_depth >= 10) workers_to_add = 4;
        else if (queue_depth >= 4) workers_to_add = 2;

        if (pool->current_threads + workers_to_add > pool->max_threads) {
            workers_to_add = pool->max_threads - pool->current_threads;
        }

        int added = 0;
        for (int i = 0; i < pool->max_threads && added < workers_to_add; i++) {
            if (!pool->workers[i].is_alive) {
                if (spawn_worker_slot(pool, i, false)) {
                    pool->current_threads++;
                    pool->total_scale_up_events++;
                    added++;
                }
            }
        }

        if (added > 0) {
            pool->last_scale_time_ms = now_ms;
            printf("  [AUTOSCALER - SCALE UP] Backlog detected (Queue: %d, Busy: %d). Spawned %d worker(s). Total workers: %d\n",
                   queue_depth, pool->busy_threads, added, pool->current_threads);

            char event_msg[128];
            snprintf(event_msg, sizeof(event_msg),
                     "Backlog detected (Queue: %d). Autoscaler spawned +%d workers. Active total: %d / %d",
                     queue_depth, added, pool->current_threads, pool->max_threads);
            db_log_event("THREAD_SCALED_UP", "WARNING", event_msg, "AUTOSCALER");
        }
    }

    pthread_mutex_unlock(&pool->pool_lock);
    return true;
}

void thread_pool_record_completion(DynamicThreadPool *pool, int turnaround_ms, bool success) {
    if (!pool) return;
    pthread_mutex_lock(&pool->pool_lock);

    if (success) {
        pool->total_requests_succeeded++;
    } else {
        pool->total_requests_failed++;
    }

    pool->total_turnaround_time_ms += turnaround_ms;

    if (pool->latency_sample_count < LATENCY_SAMPLES_CAP) {
        pool->latency_samples[pool->latency_sample_count++] = turnaround_ms;
    } else {
        pool->latency_samples[pool->latency_sample_idx] = turnaround_ms;
        pool->latency_sample_idx = (pool->latency_sample_idx + 1) % LATENCY_SAMPLES_CAP;
    }

    pthread_mutex_unlock(&pool->pool_lock);
}

void thread_pool_print_stats(DynamicThreadPool *pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->pool_lock);

    int active = pool->busy_threads;
    int current = pool->current_threads;
    int idle = current - active;
    int queued = pool->queue.count;
    long reqs = pool->total_requests_succeeded;
    long failed = pool->total_requests_failed;
    long scale_ups = pool->total_scale_up_events;
    long scale_downs = pool->total_scale_down_events;
    double avg_resp = reqs > 0 ? (pool->total_turnaround_time_ms / reqs) : 0.0;

    double p50 = 0.0;
    double p95 = 0.0;
    if (pool->latency_sample_count > 0) {
        int temp[LATENCY_SAMPLES_CAP];
        int count = pool->latency_sample_count;
        memcpy(temp, pool->latency_samples, count * sizeof(int));
        qsort(temp, count, sizeof(int), compare_ints);
        p50 = temp[(int)(count * 0.50)];
        int p95_idx = (int)(count * 0.95);
        if (p95_idx >= count) p95_idx = count - 1;
        p95 = temp[p95_idx];
    }

    printf("\n+=================[ SERVER HEALTH & OS METRICS ]=================+\n");
    printf("| Total Workers Alive : %-2d (Min: %d, Max: %d)                     |\n", current, pool->min_threads, pool->max_threads);
    printf("| Active (Busy)       : %-2d                                         |\n", active);
    printf("| Idle Standby        : %-2d                                         |\n", idle);
    printf("| Pending Queue Depth : %-2d requests (Capacity: %d)                |\n", queued, QUEUE_CAPACITY);
    printf("| Requests Succeeded  : %-6ld | Failed (Overload/Err): %-4ld       |\n", reqs, failed);
    printf("| Autoscaler Events   : Up: %ld | Down: %ld                          |\n", scale_ups, scale_downs);
    printf("| Turnaround Latency  : Avg: %-5.1f ms | P50: %-5.1f ms | P95: %-5.1f ms|\n", avg_resp, p50, p95);
    printf("+================================================================+\n\n");

    pthread_mutex_unlock(&pool->pool_lock);
}

int thread_pool_get_stats_json(DynamicThreadPool *pool, char *buffer, int max_len) {
    if (!pool || !buffer || max_len <= 0) return 0;

    pthread_mutex_lock(&pool->pool_lock);

    int active = pool->busy_threads;
    int current = pool->current_threads;
    int queued = pool->queue.count;
    long reqs = pool->total_requests_succeeded;
    long failed = pool->total_requests_failed;
    long scale_ups = pool->total_scale_up_events;
    long scale_downs = pool->total_scale_down_events;
    double avg_resp = reqs > 0 ? (pool->total_turnaround_time_ms / reqs) : 0.0;

    double p50 = 0.0;
    double p95 = 0.0;
    if (pool->latency_sample_count > 0) {
        int temp[LATENCY_SAMPLES_CAP];
        int count = pool->latency_sample_count;
        memcpy(temp, pool->latency_samples, count * sizeof(int));
        qsort(temp, count, sizeof(int), compare_ints);
        p50 = temp[(int)(count * 0.50)];
        int p95_idx = (int)(count * 0.95);
        if (p95_idx >= count) p95_idx = count - 1;
        p95 = temp[p95_idx];
    }

    int sem_emp = queue_get_sem_empty_val(&pool->queue);
    int sem_ful = queue_get_sem_full_val(&pool->queue);
    char items_buf[2048];
    queue_get_items_json(&pool->queue, items_buf, sizeof(items_buf));

    int offset = snprintf(buffer, max_len,
        "{\n"
        "  \"current_threads\": %d,\n"
        "  \"busy_threads\": %d,\n"
        "  \"idle_threads\": %d,\n"
        "  \"min_threads\": %d,\n"
        "  \"max_threads\": %d,\n"
        "  \"queue_depth\": %d,\n"
        "  \"queue_capacity\": %d,\n"
        "  \"sem_empty\": %d,\n"
        "  \"sem_full\": %d,\n"
        "  \"total_requests\": %ld,\n"
        "  \"total_succeeded\": %ld,\n"
        "  \"total_failed\": %ld,\n"
        "  \"scale_up\": %ld,\n"
        "  \"scale_down\": %ld,\n"
        "  \"avg_time\": %.2f,\n"
        "  \"p50_latency_ms\": %.1f,\n"
        "  \"p95_latency_ms\": %.1f,\n"
        "  \"queued_items\": %s,\n"
        "  \"workers\": [\n",
        current, active, (current - active), pool->min_threads, pool->max_threads,
        queued, QUEUE_CAPACITY, sem_emp, sem_ful,
        (reqs + failed), reqs, failed, scale_ups, scale_downs, avg_resp, p50, p95, items_buf);

    for (int i = 0; i < pool->max_threads && offset < max_len - 150; i++) {
        offset += snprintf(buffer + offset, max_len - offset,
            "    {\"id\": %d, \"alive\": %s, \"busy\": %s, \"baseline\": %s}%s\n",
            pool->workers[i].worker_id,
            pool->workers[i].is_alive ? "true" : "false",
            pool->workers[i].is_busy ? "true" : "false",
            pool->workers[i].is_baseline ? "true" : "false",
            (i < pool->max_threads - 1) ? "," : "");
    }

    offset += snprintf(buffer + offset, max_len - offset, "  ]\n}\n");

    pthread_mutex_unlock(&pool->pool_lock);
    return offset;
}

int thread_pool_get_workers_json(DynamicThreadPool *pool, char *buffer, int max_len) {
    if (!pool || !buffer || max_len <= 0) return 0;

    pthread_mutex_lock(&pool->pool_lock);

    int active = pool->busy_threads;
    int current = pool->current_threads;
    long long now_ms = current_timestamp_ms();

    int offset = snprintf(buffer, max_len,
        "{\"min_workers\":%d,\"max_workers\":%d,\"current_workers\":%d,\"busy_workers\":%d,\"idle_workers\":%d,\"workers\":[",
        pool->min_threads, pool->max_threads, current, active, (current - active));

    for (int i = 0; i < pool->max_threads && offset < max_len - 250; i++) {
        WorkerThread *w = &pool->workers[i];
        const char *state_str = "RETIRED";
        if (w->is_alive) {
            state_str = w->is_busy ? "BUSY" : "IDLE";
        }

        long long idle_sec = (w->is_alive && !w->is_busy) ? (now_ms - w->last_busy_time_ms) / 1000 : 0;
        if (idle_sec < 0) idle_sec = 0;

        offset += snprintf(buffer + offset, max_len - offset,
            "%s{\"id\":%d,\"name\":\"Worker W%d\",\"alive\":%s,\"busy\":%s,\"baseline\":%s,\"state\":\"%s\","
            "\"processed_count\":%ld,\"current_task\":\"%s\",\"current_patient\":\"%s\",\"current_priority\":%d,\"idle_seconds\":%lld}",
            (i > 0 ? "," : ""),
            w->worker_id,
            w->worker_id + 1,
            w->is_alive ? "true" : "false",
            w->is_busy ? "true" : "false",
            w->is_baseline ? "true" : "false",
            state_str,
            w->requests_processed,
            strlen(w->current_task_id) > 0 ? w->current_task_id : "-",
            strlen(w->current_patient) > 0 ? w->current_patient : "-",
            w->current_priority,
            idle_sec);
    }

    offset += snprintf(buffer + offset, max_len - offset, "]}");
    pthread_mutex_unlock(&pool->pool_lock);
    return offset;
}

void thread_pool_destroy(DynamicThreadPool *pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->pool_lock);
    pool->is_shutdown = true;
    pthread_mutex_unlock(&pool->pool_lock);

    // Unblock consumers sleeping in queue
    queue_shutdown(&pool->queue);

    // Join all workers that are currently alive or exited but not yet joined
    for (int i = 0; i < pool->max_threads; i++) {
        if (pool->workers[i].has_thread) {
            pthread_join(pool->workers[i].thread_id, NULL);
            pool->workers[i].has_thread = false;
            pool->workers[i].is_alive = false;
        }
    }

    queue_destroy(&pool->queue);
    pthread_mutex_destroy(&pool->pool_lock);
    free(pool);
    printf("[THREAD POOL] Destroyed and all worker threads joined cleanly.\n");
}
