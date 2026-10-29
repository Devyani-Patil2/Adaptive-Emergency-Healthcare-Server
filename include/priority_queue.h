#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"

#define QUEUE_CAPACITY 64

/*
 * Thread-Safe Bounded Priority Queue
 * 
 * OS Concepts Demonstrated:
 * 1. Producer-Consumer Architecture:
 *    - Master listener produces Tasks into the bounded buffer.
 *    - Worker threads consume Tasks from the bounded buffer.
 * 2. Counting Semaphores:
 *    - sem_empty: Counting semaphore tracking available buffer slots.
 *    - sem_full:  Counting semaphore tracking occupied task slots.
 * 3. Mutex:
 *    - Binary mutual exclusion lock protecting critical section during enqueue/dequeue.
 * 4. Priority Scheduling & Aging:
 *    - Priority levels P0 to P3 with arrival-time aging to eliminate starvation.
 */
typedef struct {
    Task tasks[QUEUE_CAPACITY];
    int count;
    bool is_shutdown;
    
    pthread_mutex_t mutex;
    sem_t sem_empty;  // Tracks available slots for Producer
    sem_t sem_full;   // Tracks available tasks for Consumer
} PriorityTaskQueue;

/* Queue Functions */
void queue_init(PriorityTaskQueue *q);
void queue_destroy(PriorityTaskQueue *q);
void queue_shutdown(PriorityTaskQueue *q);

/* Non-blocking push: returns false immediately if queue full (HTTP 503) */
bool queue_push_try(PriorityTaskQueue *q, Task task);

/* Blocking push */
bool queue_push(PriorityTaskQueue *q, Task task);

/* Popping functions with aging-aware priority scheduling */
bool queue_pop(PriorityTaskQueue *q, Task *out_task);
bool queue_pop_timed(PriorityTaskQueue *q, Task *out_task, int timeout_seconds);

/* Query queue count in a thread-safe manner */
int queue_get_count(PriorityTaskQueue *q);

#endif // PRIORITY_QUEUE_H
