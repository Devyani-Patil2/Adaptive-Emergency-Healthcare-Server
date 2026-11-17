#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"

#define QUEUE_CAPACITY 64

/*
 * Thread-Safe Priority-Based Bounded Queue (Ring Buffer)
 * Coordinates master listener thread (Producer) and worker threads (Consumers).
 * 
 * Synchronized using:
 *  - sem_empty: Counting semaphore tracking available empty slots in buffer (init: 64)
 *  - sem_full:  Counting semaphore tracking available ready tasks for workers (init: 0)
 *  - mutex:     Mutual exclusion lock protecting queue array, count and indices
 */
typedef struct {
    Task tasks[QUEUE_CAPACITY];
    int count;
    pthread_mutex_t mutex;
    sem_t sem_empty; // Producer waits if 0 (bounded buffer capacity full)
    sem_t sem_full;  // Consumer waits if 0 (no tasks available)
    bool is_shutdown;
} PriorityTaskQueue;

void queue_init(PriorityTaskQueue *q);
void queue_destroy(PriorityTaskQueue *q);
void queue_shutdown(PriorityTaskQueue *q);

/* Non-blocking push: returns false immediately if full, triggering HTTP 503 */
bool queue_push_try(PriorityTaskQueue *q, Task task);

/* Blocking push with retry on interrupt */
bool queue_push(PriorityTaskQueue *q, Task task);

/* Timed pop: selects highest effective priority task (with aging) */
bool queue_pop_timed(PriorityTaskQueue *q, Task *out_task, int timeout_seconds);
bool queue_pop(PriorityTaskQueue *q, Task *out_task);

int queue_get_count(PriorityTaskQueue *q);
int queue_get_sem_empty_val(PriorityTaskQueue *q);
int queue_get_sem_full_val(PriorityTaskQueue *q);
int queue_get_items_json(PriorityTaskQueue *q, char *buf, size_t max_len);
int queue_get_detailed_json(PriorityTaskQueue *q, char *buf, size_t max_len);

#endif // PRIORITY_QUEUE_H
