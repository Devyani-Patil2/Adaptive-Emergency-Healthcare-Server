#ifndef SERVICE_HANDLER_H
#define SERVICE_HANDLER_H

#include "common.h"

// Forward declaration
typedef struct DynamicThreadPool DynamicThreadPool;

/* Processes an individual HTTP/Healthcare task inside a worker thread */
void process_healthcare_task(Task *task, int worker_id, DynamicThreadPool *pool);

#endif // SERVICE_HANDLER_H
