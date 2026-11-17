#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>
#include <stdbool.h>

#define DB_DEFAULT_PATH "healthcare.db"

/* Initialize SQLite database, applying schema and seeding if empty */
int db_init(const char *db_path);

/* Close database connection cleanly */
void db_close(void);

/* Query functions returning pre-formatted JSON */
int db_get_hospitals_json(char *buf, size_t max_len);
int db_get_doctors_json(char *buf, size_t max_len);
int db_get_slots_json(int doctor_id, char *buf, size_t max_len);
int db_get_appointments_json(int limit, char *buf, size_t max_len);
int db_get_appointment_by_id_json(int appointment_id, char *buf, size_t max_len);
int db_get_emergency_info_json(char *buf, size_t max_len);

/*
 * Atomically book an appointment slot.
 * Returns:
 *   0  -> Success (out_booking_code populated)
 *   1  -> Slot conflict (already booked) -> Map to HTTP 409
 *  -1  -> Validation error (invalid doctor or slot) -> Map to HTTP 400
 *  -2  -> Internal SQLite failure -> Map to HTTP 500
 */
int db_book_appointment(int doctor_id, int slot_id, const char *patient_name,
                          const char *patient_phone, const char *notes,
                          int worker_id, int turnaround_ms,
                          char *out_booking_code, size_t code_len,
                          char *err_msg, size_t err_len);

int db_reset_slot_for_demo(int slot_id);
int db_reset_queue(void);
int db_update_appointment_status(const char *booking_code, const char *status);
int db_get_least_loaded_doctor_id(void);

/* Patient & Triage Requests Persistence */
int db_record_patient(const char *patient_id, const char *name, int age,
                      const char *gender, const char *phone);

int db_record_request(const char *req_id, const char *patient_id, const char *patient_name,
                      int priority, const char *priority_name, const char *symptoms,
                      const char *vitals, const char *priority_reason, const char *required_resource);

int db_update_request_status(const char *req_id, const char *status, int worker_id, int queue_wait_ms);

int db_get_requests_json(int limit, char *buf, size_t max_len);

/* Hospital Resources Management */
int db_get_resources_json(char *buf, size_t max_len);

int db_allocate_resource(const char *resource_type, const char *patient_id, const char *patient_name,
                         char *out_code, size_t code_len, char *err_msg, size_t err_len);

int db_release_resource(const char *allocation_code, const char *resource_type,
                        char *err_msg, size_t err_len);

int db_get_allocations_json(int limit, char *buf, size_t max_len);

/* System Events / Notifications Log */
int db_log_event(const char *event_type, const char *severity,
                 const char *message, const char *related_id);

int db_get_events_json(int limit, char *buf, size_t max_len);

#endif // DATABASE_H
