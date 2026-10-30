#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 4096
#define AGING_THRESHOLD_MS 2000  // Promote priority level for every 2000ms of queue wait

/* 
 * Healthcare Service Types (Priority Levels for Scheduling)
 * Priority 0 (Highest) -> REQ_EMERGENCY_ACCIDENT (Road Trauma, Cardiac Arrest, ICU)
 * Priority 1           -> REQ_EMERGENCY_INFO     (Critical Beds, Oxygen Lookup)
 * Priority 2           -> REQ_TELEMEDICINE       (Urgent Doctor Video Call)
 * Priority 3           -> REQ_VACCINATION        (Outbreak Immunization)
 * Priority 4 (Lowest)  -> REQ_APPOINTMENT        (Routine OPD Consultation)
 */
typedef enum {
    REQ_EMERGENCY_ACCIDENT = 0,
    REQ_EMERGENCY_INFO     = 1,
    REQ_TELEMEDICINE       = 2,
    REQ_VACCINATION        = 3,
    REQ_APPOINTMENT        = 4
} RequestType;

/* Structure representing a single client request processed by worker threads */
typedef struct {
    int client_socket;
    RequestType type;
    struct timeval arrival_time;
    
    // HTTP details
    char http_method[16];
    char path[128];
    char body[512];
    
    // Structured healthcare parameters
    int doctor_id;
    int slot_id;
    char patient_name[64];
    char patient_phone[32];
    char notes[128];
} Task;

/* Convert request type to human-readable string */
static inline const char* request_type_to_str(RequestType type) {
    switch (type) {
        case REQ_EMERGENCY_ACCIDENT: return "ACCIDENT/TRAUMA (P0 - Life Threatening)";
        case REQ_EMERGENCY_INFO:     return "ICU/EMERGENCY   (P1 - Critical)";
        case REQ_TELEMEDICINE:       return "TELEMEDICINE   (P2 - High)";
        case REQ_VACCINATION:        return "VACCINATION    (P3 - Medium)";
        case REQ_APPOINTMENT:        return "ROUTINE OPD    (P4 - Normal)";
        default:                     return "UNKNOWN";
    }
}

/* Helper to get current timestamp in milliseconds */
static inline long long current_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

/*
 * Calculate effective priority considering wait time (Aging Mechanism).
 * Prevents starvation of routine appointments (P3) during emergency surges.
 */
static inline int calculate_effective_priority(const Task *task, long long now_ms) {
    long long wait_ms = now_ms - ((long long)task->arrival_time.tv_sec * 1000 + 
                                  task->arrival_time.tv_usec / 1000);
    if (wait_ms < 0) wait_ms = 0;

    int boost_levels = (int)(wait_ms / AGING_THRESHOLD_MS);
    int effective = (int)task->type - boost_levels;
    if (effective < (int)REQ_EMERGENCY_ACCIDENT) {
        effective = (int)REQ_EMERGENCY_ACCIDENT;
    }
    return effective;
}

#endif // COMMON_H
