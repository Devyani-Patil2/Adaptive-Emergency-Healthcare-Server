#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"
#include "priority_queue.h"

#define MIN_WORKERS_ALLOWED 1
#define MAX_WORKERS_LIMIT 64
#define DEFAULT_MIN_THREADS 4
#define DEFAULT_MAX_THREADS 16
#define DEFAULT_IDLE_TIMEOUT_SEC 4
#define SCALE_COOLDOWN_MS 150
#define LATENCY_SAMPLES_CAP 1000

typedef struct DynamicThreadPool DynamicThreadPool;

/* Structure representing individual worker thread metadata */
typedef struct {
    pthread_t thread_id;
    int worker_id;
    bool has_thread;          // True if thread was spawned and needs pthread_join
    bool is_alive;
    bool is_busy;
    bool is_baseline;         // Baseline workers never retire on idle
    long long last_busy_time_ms;
    long long spawn_time_ms;
    long requests_processed;
    char current_task_id[32];
    char current_patient[64];
    int current_priority;
    DynamicThreadPool *pool;
} WorkerThread;

/*
 * Dynamic Thread Pool Structure
 *
 * OS Concepts Demonstrated:
 * 1. Multithreading & Thread Lifecycle Management:
 *    - Creation, execution, dynamic termination, and deterministic joining.
 * 2. Adaptive Resource Allocation:
 *    - Autoscaling from MIN_THREADS to MAX_THREADS during traffic bursts.
 *    - Hysteresis & cooldown to prevent flapping.
 *    - Graceful idle thread pruning during low traffic to save CPU/RAM.
 * 3. Synchronization & Concurrency:
 *    - Mutex locks protecting worker state arrays and metrics.
 * 4. Preventing Context-Switching Thrashing:
 *    - Capping workers at MAX_THREADS prevents context-switching storms.
 */
struct DynamicThreadPool {
    int min_threads;
    int max_threads;
    int idle_timeout_sec;
    
    int current_threads;
    int busy_threads;
    bool is_shutdown;

    WorkerThread workers[MAX_WORKERS_LIMIT];
    PriorityTaskQueue queue;

    pthread_mutex_t pool_lock;
    long long last_scale_time_ms;

    // Metrics for OS evaluation and performance reporting
    long total_requests_submitted;
    long total_requests_succeeded;
    long total_requests_failed;
    long total_scale_up_events;
    long total_scale_down_events;
    double total_turnaround_time_ms;

    // Latency distribution samples for accurate P50 & P95 calculation
    int latency_sample_count;
    int latency_sample_idx;
    int latency_samples[LATENCY_SAMPLES_CAP];
};

/* Thread Pool API */
DynamicThreadPool* thread_pool_create(int min_threads, int max_threads, int idle_timeout);
void thread_pool_destroy(DynamicThreadPool *pool);
bool thread_pool_submit(DynamicThreadPool *pool, Task task);
void thread_pool_print_stats(DynamicThreadPool *pool);
int thread_pool_get_stats_json(DynamicThreadPool *pool, char *buffer, int max_len);
int thread_pool_get_workers_json(DynamicThreadPool *pool, char *buffer, int max_len);

/* Metric recording */
void thread_pool_record_completion(DynamicThreadPool *pool, int turnaround_ms, bool success);

#endif // THREAD_POOL_H
