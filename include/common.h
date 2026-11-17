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
 * Internal application scheduling classes aligned with clinical urgency:
 * Priority 0 (Highest) -> REQ_P0_CRITICAL    (Critical Emergency: Severe accident, trauma, cardiac arrest, unconscious)
 * Priority 1           -> REQ_P1_HIGH        (High Priority: Serious injuries, severe pain, urgent medical attention)
 * Priority 2           -> REQ_P2_MODERATE    (Moderate Priority: Stable injuries, telemedicine, moderate symptoms)
 * Priority 3 (Lowest)  -> REQ_P3_ROUTINE     (Low Priority: Routine consultation, general checkups, OPD appointments)
 */
typedef enum {
    REQ_P0_CRITICAL   = 0,
    REQ_P1_HIGH       = 1,
    REQ_P2_MODERATE   = 2,
    REQ_P3_ROUTINE    = 3,
    
    // Compatibility aliases
    REQ_EMERGENCY_ACCIDENT = 0,
    REQ_EMERGENCY_INFO     = 1,
    REQ_TELEMEDICINE       = 2,
    REQ_VACCINATION        = 3,
    REQ_APPOINTMENT        = 3
} RequestType;

/* Structure representing a single client request processed by worker threads */
typedef struct {
    int client_socket;
    RequestType type;
    struct timeval arrival_time;
    
    // HTTP details
    char http_method[16];
    char path[128];
    char body[1024];
    
    // Structured healthcare parameters
    int doctor_id;
    int slot_id;
    char patient_id[32];
    char request_id[32];
    char patient_name[64];
    int patient_age;
    char patient_gender[16];
    char patient_phone[32];
    char symptoms[128];
    char vitals[128];
    char priority_reason[128];
    char required_resource[64];
    char notes[128];
} Task;

/* Convert request type to human-readable string */
static inline const char* request_type_to_str(RequestType type) {
    switch (type) {
        case REQ_P0_CRITICAL: return "P0 - Critical Emergency";
        case REQ_P1_HIGH:     return "P1 - High Priority";
        case REQ_P2_MODERATE: return "P2 - Moderate Priority";
        case REQ_P3_ROUTINE:  return "P3 - Routine Consultation";
        default:              return "P3 - Routine Consultation";
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
 * CRITICAL RULE: Aging NEVER overrides genuine P0 emergency precedence!
 * P0 always stays at 0. Lower priority tasks (P1, P2, P3) can age up to minimum 1.
 */
static inline int calculate_effective_priority(const Task *task, long long now_ms) {
    if (task->type == REQ_P0_CRITICAL) {
        return 0; // Critical emergencies always retain absolute top precedence
    }
    
    long long wait_ms = now_ms - ((long long)task->arrival_time.tv_sec * 1000 + 
                                  task->arrival_time.tv_usec / 1000);
    if (wait_ms < 0) wait_ms = 0;

    int boost_levels = (int)(wait_ms / AGING_THRESHOLD_MS);
    int effective = (int)task->type - boost_levels;
    
    // Lower priority tasks can age up to 1, but never surpass true P0 emergencies
    if (effective < 1) {
        effective = 1;
    }
    return effective;
}

#endif // COMMON_H
