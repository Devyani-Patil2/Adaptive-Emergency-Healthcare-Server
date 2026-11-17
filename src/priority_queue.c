#include "priority_queue.h"

void queue_init(PriorityTaskQueue *q) {
    q->count = 0;
    q->is_shutdown = false;
    pthread_mutex_init(&q->mutex, NULL);
    
    // Counting Semaphore 1: Tracks available empty slots for producer
    sem_init(&q->sem_empty, 0, QUEUE_CAPACITY);
    
    // Counting Semaphore 2: Tracks available tasks for consumers
    sem_init(&q->sem_full, 0, 0);
}

void queue_shutdown(PriorityTaskQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->is_shutdown = true;
    pthread_mutex_unlock(&q->mutex);

    // Unblock any workers sleeping on empty queue
    for (int i = 0; i < QUEUE_CAPACITY; i++) {
        sem_post(&q->sem_full);
    }
}

void queue_destroy(PriorityTaskQueue *q) {
    queue_shutdown(q);
    pthread_mutex_destroy(&q->mutex);
    sem_destroy(&q->sem_empty);
    sem_destroy(&q->sem_full);
}

/*
 * Non-blocking push for the server accept loop.
 * If the queue is at capacity (64 requests), returns false immediately
 * so the server can reply with HTTP 503 instead of freezing.
 */
bool queue_push_try(PriorityTaskQueue *q, Task task) {
    if (sem_trywait(&q->sem_empty) != 0) {
        return false; // Queue is full (no empty slots available)
    }

    pthread_mutex_lock(&q->mutex);
    if (q->is_shutdown || q->count >= QUEUE_CAPACITY) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->sem_empty);
        return false;
    }

    q->tasks[q->count++] = task;
    pthread_mutex_unlock(&q->mutex);

    sem_post(&q->sem_full);
    return true;
}

/* Blocking push with EINTR retry */
bool queue_push(PriorityTaskQueue *q, Task task) {
    int rc;
    do {
        rc = sem_wait(&q->sem_empty);
    } while (rc != 0 && errno == EINTR);

    if (rc != 0) return false;

    pthread_mutex_lock(&q->mutex);
    if (q->is_shutdown || q->count >= QUEUE_CAPACITY) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->sem_empty);
        return false;
    }

    q->tasks[q->count++] = task;
    pthread_mutex_unlock(&q->mutex);

    sem_post(&q->sem_full);
    return true;
}

/*
 * Consumer worker calls this with a timeout.
 * Dequeues the task with the highest effective priority, taking Aging into account.
 */
bool queue_pop_timed(PriorityTaskQueue *q, Task *out_task, int timeout_seconds) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_seconds;

    int rc;
    do {
        rc = sem_timedwait(&q->sem_full, &ts);
    } while (rc != 0 && errno == EINTR);

    if (rc != 0) {
        return false; // Timed out (ETIMEDOUT) or error
    }

    pthread_mutex_lock(&q->mutex);

    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    // Dynamic Priority Scheduling with Aging Evaluation
    long long now_ms = current_timestamp_ms();
    int best_idx = 0;
    int best_priority = calculate_effective_priority(&q->tasks[0], now_ms);
    long long best_arrival = (long long)q->tasks[0].arrival_time.tv_sec * 1000 + 
                             q->tasks[0].arrival_time.tv_usec / 1000;

    for (int i = 1; i < q->count; i++) {
        int eff = calculate_effective_priority(&q->tasks[i], now_ms);
        long long arr = (long long)q->tasks[i].arrival_time.tv_sec * 1000 + 
                        q->tasks[i].arrival_time.tv_usec / 1000;
        
        // Lower numeric value = higher urgency. FIFO tie-breaking on equal priority.
        if (eff < best_priority || (eff == best_priority && arr < best_arrival)) {
            best_idx = i;
            best_priority = eff;
            best_arrival = arr;
        }
    }

    *out_task = q->tasks[best_idx];

    // Remove selected task by shifting remaining items
    for (int i = best_idx; i < q->count - 1; i++) {
        q->tasks[i] = q->tasks[i + 1];
    }
    q->count--;

    pthread_mutex_unlock(&q->mutex);

    // Signal producer that a buffer slot is now free
    sem_post(&q->sem_empty);
    return true;
}

bool queue_pop(PriorityTaskQueue *q, Task *out_task) {
    int rc;
    do {
        rc = sem_wait(&q->sem_full);
    } while (rc != 0 && errno == EINTR);

    if (rc != 0) return false;

    pthread_mutex_lock(&q->mutex);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    long long now_ms = current_timestamp_ms();
    int best_idx = 0;
    int best_priority = calculate_effective_priority(&q->tasks[0], now_ms);
    long long best_arrival = (long long)q->tasks[0].arrival_time.tv_sec * 1000 + 
                             q->tasks[0].arrival_time.tv_usec / 1000;

    for (int i = 1; i < q->count; i++) {
        int eff = calculate_effective_priority(&q->tasks[i], now_ms);
        long long arr = (long long)q->tasks[i].arrival_time.tv_sec * 1000 + 
                        q->tasks[i].arrival_time.tv_usec / 1000;
        if (eff < best_priority || (eff == best_priority && arr < best_arrival)) {
            best_idx = i;
            best_priority = eff;
            best_arrival = arr;
        }
    }

    *out_task = q->tasks[best_idx];
    for (int i = best_idx; i < q->count - 1; i++) {
        q->tasks[i] = q->tasks[i + 1];
    }
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->sem_empty);
    return true;
}

int queue_get_count(PriorityTaskQueue *q) {
    pthread_mutex_lock(&q->mutex);
    int count = q->count;
    pthread_mutex_unlock(&q->mutex);
    return count;
}

int queue_get_sem_empty_val(PriorityTaskQueue *q) {
    int val = 0;
    sem_getvalue(&q->sem_empty, &val);
    if (val < 0) val = 0;
    return val;
}

int queue_get_sem_full_val(PriorityTaskQueue *q) {
    int val = 0;
    sem_getvalue(&q->sem_full, &val);
    if (val < 0) val = 0;
    return val;
}

int queue_get_items_json(PriorityTaskQueue *q, char *buf, size_t max_len) {
    pthread_mutex_lock(&q->mutex);
    long long now_ms = current_timestamp_ms();
    size_t offset = snprintf(buf, max_len, "[");
    for (int i = 0; i < q->count && i < 16 && offset < max_len - 150; i++) {
        long long arr_ms = (long long)q->tasks[i].arrival_time.tv_sec * 1000 + (q->tasks[i].arrival_time.tv_usec / 1000);
        long long wait_ms = now_ms - arr_ms;
        if (wait_ms < 0) wait_ms = 0;
        int eff = calculate_effective_priority(&q->tasks[i], now_ms);
        int boost = (int)q->tasks[i].type - eff;
        offset += snprintf(buf + offset, max_len - offset,
            "{\"idx\":%d,\"type\":%d,\"type_name\":\"%s\",\"base_prio\":%d,\"eff_prio\":%d,\"wait_ms\":%lld,\"boost\":%d,\"patient\":\"%s\"}%s",
            i, q->tasks[i].type, request_type_to_str(q->tasks[i].type),
            q->tasks[i].type, eff, wait_ms, boost, q->tasks[i].patient_name,
            (i < q->count - 1 && i < 15) ? "," : "");
    }
    offset += snprintf(buf + offset, max_len - offset, "]");
    pthread_mutex_unlock(&q->mutex);
    return (int)offset;
}

int queue_get_detailed_json(PriorityTaskQueue *q, char *buf, size_t max_len) {
    pthread_mutex_lock(&q->mutex);
    long long now_ms = current_timestamp_ms();
    int sem_emp = 0, sem_ful = 0;
    sem_getvalue(&q->sem_empty, &sem_emp);
    sem_getvalue(&q->sem_full, &sem_ful);
    if (sem_emp < 0) sem_emp = 0;
    if (sem_ful < 0) sem_ful = 0;

    size_t offset = snprintf(buf, max_len,
        "{\"count\":%d,\"capacity\":%d,\"sem_empty\":%d,\"sem_full\":%d,\"items\":[",
        q->count, QUEUE_CAPACITY, sem_emp, sem_ful);

    for (int i = 0; i < q->count && offset < max_len - 250; i++) {
        long long arr_ms = (long long)q->tasks[i].arrival_time.tv_sec * 1000 + (q->tasks[i].arrival_time.tv_usec / 1000);
        long long wait_ms = now_ms - arr_ms;
        if (wait_ms < 0) wait_ms = 0;
        int eff = calculate_effective_priority(&q->tasks[i], now_ms);
        int boost = (int)q->tasks[i].type - eff;

        offset += snprintf(buf + offset, max_len - offset,
            "%s{\"index\":%d,\"request_id\":\"%s\",\"patient_id\":\"%s\",\"patient_name\":\"%s\","
            "\"type\":%d,\"priority_label\":\"%s\",\"base_priority\":%d,\"effective_priority\":%d,"
            "\"wait_time_ms\":%lld,\"aging_boost\":%d,\"symptoms\":\"%s\",\"priority_reason\":\"%s\","
            "\"required_resource\":\"%s\"}",
            (i > 0 ? "," : ""),
            i + 1,
            strlen(q->tasks[i].request_id) > 0 ? q->tasks[i].request_id : "REQ-WAIT",
            strlen(q->tasks[i].patient_id) > 0 ? q->tasks[i].patient_id : "PAT-WAIT",
            q->tasks[i].patient_name,
            (int)q->tasks[i].type,
            request_type_to_str(q->tasks[i].type),
            (int)q->tasks[i].type,
            eff,
            wait_ms,
            boost,
            q->tasks[i].symptoms,
            q->tasks[i].priority_reason,
            q->tasks[i].required_resource);
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    pthread_mutex_unlock(&q->mutex);
    return (int)offset;
}
