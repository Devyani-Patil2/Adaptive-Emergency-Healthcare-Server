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

#endif // DATABASE_H
