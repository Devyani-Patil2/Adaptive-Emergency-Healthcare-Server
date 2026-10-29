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
