#include "database.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static sqlite3 *g_db = NULL;
static pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *SCHEMA_SQL =
    "PRAGMA foreign_keys = ON;"
    "CREATE TABLE IF NOT EXISTS hospitals ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    name TEXT NOT NULL,"
    "    address TEXT NOT NULL,"
    "    distance_km REAL NOT NULL,"
    "    icu_beds_total INTEGER NOT NULL,"
    "    icu_beds_available INTEGER NOT NULL,"
    "    oxygen_percent INTEGER NOT NULL,"
    "    phone TEXT NOT NULL,"
    "    is_emergency_active INTEGER NOT NULL DEFAULT 1"
    ");"
    "CREATE TABLE IF NOT EXISTS doctors ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    name TEXT NOT NULL,"
    "    speciality TEXT NOT NULL,"
    "    experience_years INTEGER NOT NULL,"
    "    fee_usd INTEGER NOT NULL,"
    "    rating REAL NOT NULL DEFAULT 4.9,"
    "    photo_url TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS appointment_slots ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    doctor_id INTEGER NOT NULL,"
    "    day_of_week TEXT NOT NULL,"
    "    start_time TEXT NOT NULL,"
    "    end_time TEXT NOT NULL,"
    "    is_booked INTEGER NOT NULL DEFAULT 0,"
    "    FOREIGN KEY(doctor_id) REFERENCES doctors(id) ON DELETE CASCADE,"
    "    UNIQUE(doctor_id, day_of_week, start_time)"
    ");"
    "CREATE TABLE IF NOT EXISTS appointments ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    booking_code TEXT NOT NULL UNIQUE,"
    "    doctor_id INTEGER NOT NULL,"
    "    slot_id INTEGER NOT NULL,"
    "    patient_name TEXT NOT NULL,"
    "    patient_phone TEXT NOT NULL,"
    "    notes TEXT,"
    "    status TEXT NOT NULL DEFAULT 'CONFIRMED',"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    handled_by_worker INTEGER NOT NULL,"
    "    turnaround_ms INTEGER NOT NULL,"
    "    FOREIGN KEY(doctor_id) REFERENCES doctors(id),"
    "    FOREIGN KEY(slot_id) REFERENCES appointment_slots(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS patients ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    patient_id TEXT NOT NULL UNIQUE,"
    "    name TEXT NOT NULL,"
    "    age INTEGER NOT NULL,"
    "    gender TEXT,"
    "    phone TEXT NOT NULL,"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE TABLE IF NOT EXISTS requests ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    request_id TEXT NOT NULL UNIQUE,"
    "    patient_id TEXT NOT NULL,"
    "    patient_name TEXT NOT NULL,"
    "    priority INTEGER NOT NULL,"
    "    priority_name TEXT NOT NULL,"
    "    symptoms TEXT,"
    "    vitals TEXT,"
    "    priority_reason TEXT,"
    "    required_resource TEXT,"
    "    status TEXT NOT NULL DEFAULT 'QUEUED',"
    "    worker_id INTEGER DEFAULT -1,"
    "    queue_wait_ms INTEGER DEFAULT 0,"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE TABLE IF NOT EXISTS hospital_resources ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    resource_type TEXT NOT NULL UNIQUE,"
    "    display_name TEXT NOT NULL,"
    "    total_quantity INTEGER NOT NULL,"
    "    occupied_quantity INTEGER NOT NULL DEFAULT 0,"
    "    unit TEXT NOT NULL DEFAULT 'Units'"
    ");"
    "CREATE TABLE IF NOT EXISTS resource_allocations ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    allocation_code TEXT NOT NULL UNIQUE,"
    "    patient_id TEXT NOT NULL,"
    "    patient_name TEXT NOT NULL,"
    "    resource_type TEXT NOT NULL,"
    "    status TEXT NOT NULL DEFAULT 'ALLOCATED',"
    "    allocated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    released_at TIMESTAMP"
    ");"
    "CREATE TABLE IF NOT EXISTS system_events ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    event_type TEXT NOT NULL,"
    "    severity TEXT NOT NULL,"
    "    message TEXT NOT NULL,"
    "    related_id TEXT,"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_slots_doc_avail ON appointment_slots(doctor_id, is_booked);"
    "CREATE INDEX IF NOT EXISTS idx_appointments_code ON appointments(booking_code);"
    "CREATE INDEX IF NOT EXISTS idx_requests_priority ON requests(priority, status);"
    "CREATE INDEX IF NOT EXISTS idx_events_created ON system_events(created_at DESC);";

static const char *SEED_SQL =
    "INSERT OR IGNORE INTO hospitals (name, address, distance_km, icu_beds_total, icu_beds_available, oxygen_percent, phone, is_emergency_active)"
    " VALUES"
    " ('City General Trauma Center', 'Sector 4, MG Road, Central', 1.2, 20, 14, 100, '+91-20-2550-0101', 1),"
    " ('Apollo Emergency & Critical Care', 'Bandra Kurla Complex, West', 3.5, 16, 8, 98, '+91-20-2550-0102', 1),"
    " ('Metro Multi-Specialty Hospital', 'South Avenue, Tech Park', 5.1, 30, 22, 95, '+91-20-2550-0103', 1);"
    "INSERT OR IGNORE INTO doctors (id, name, speciality, experience_years, fee_usd, rating, photo_url)"
    " VALUES"
    " (1, 'Dr. Ananya Sharma', 'Pediatric & Trauma', 12, 500, 4.9, 'https://images.unsplash.com/photo-1594824813589-9a7457788e34?w=100&auto=format&fit=crop&q=80'),"
    " (2, 'Dr. Rajesh Kulkarni', 'General Surgery & ICU', 10, 800, 4.8, 'https://images.unsplash.com/photo-1622253692010-333f2da6031d?w=100&auto=format&fit=crop&q=80'),"
    " (3, 'Dr. Priya Iyer', 'Gastroenterology', 7, 400, 4.9, 'https://images.unsplash.com/photo-1559839734-2b71ea197ec2?w=100&auto=format&fit=crop&q=80'),"
    " (4, 'Dr. Arvind Menon', 'Interventional Cardiology', 15, 1000, 5.0, 'https://images.unsplash.com/photo-1537368910025-700350fe46c7?w=100&auto=format&fit=crop&q=80');"
    "INSERT OR IGNORE INTO appointment_slots (doctor_id, day_of_week, start_time, end_time, is_booked)"
    " VALUES"
    " (1, 'Tue', '10:00 AM', '11:00 AM', 0),"
    " (1, 'Tue', '11:00 AM', '12:00 PM', 0),"
    " (1, 'Thu', '10:00 AM', '11:00 AM', 0),"
    " (1, 'Thu', '12:00 PM', '01:00 PM', 0),"
    " (2, 'Mon', '02:00 PM', '03:00 PM', 0),"
    " (2, 'Mon', '03:00 PM', '04:00 PM', 0),"
    " (2, 'Wed', '02:00 PM', '03:00 PM', 0),"
    " (2, 'Wed', '04:00 PM', '05:00 PM', 0),"
    " (3, 'Daily', '09:00 AM', '10:00 AM', 0),"
    " (3, 'Daily', '10:00 AM', '11:00 AM', 0),"
    " (3, 'Daily', '11:00 AM', '12:00 PM', 0),"
    " (4, 'Fri', '10:00 AM', '10:30 AM', 0),"
    " (4, 'Sat', '10:00 AM', '10:30 AM', 0),"
    " (4, 'Sat', '02:00 PM', '02:30 PM', 0);"
    "INSERT OR IGNORE INTO hospital_resources (resource_type, display_name, total_quantity, occupied_quantity, unit)"
    " VALUES"
    " ('oxygen_cylinders', 'High-Flow Oxygen Cylinders', 10, 4, 'Cylinders'),"
    " ('blood_units', 'Blood Bank Emergency Reserve (O-)', 8, 3, 'Units'),"
    " ('icu_beds', 'Critical Care ICU Beds', 6, 2, 'Beds'),"
    " ('ventilators', 'Mechanical Ventilators', 5, 2, 'Units'),"
    " ('general_beds', 'General Ward Beds', 20, 8, 'Beds'),"
    " ('doctors_duty', 'Emergency Doctors on Duty', 8, 3, 'Physicians');"
    "INSERT OR IGNORE INTO system_events (event_type, severity, message, related_id)"
    " VALUES"
    " ('SYSTEM_START', 'INFO', 'Adaptive Healthcare Server initialized with baseline worker threads.', 'SYS-01'),"
    " ('AUTOSCALER_READY', 'SUCCESS', 'Dynamic thread pool ready: min 4, max 16, bounded queue cap 64.', 'POOL-01'),"
    " ('DB_READY', 'INFO', 'SQLite database persistence engine mounted with ACID transaction protection.', 'DB-01'),"
    " ('RESOURCE_UPDATE', 'INFO', 'Hospital inventory initialized: 10 Oxygen Cylinders, 8 Blood Units, 6 ICU Beds, 5 Ventilators, 20 General Beds.', 'RES-01');";

int db_init(const char *db_path) {
    pthread_mutex_lock(&g_db_lock);
    if (g_db != NULL) {
        pthread_mutex_unlock(&g_db_lock);
        return 0;
    }

    const char *path = (db_path && strlen(db_path) > 0) ? db_path : DB_DEFAULT_PATH;
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Cannot open database %s: %s\n", path, sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    char *err_msg = NULL;
    rc = sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Schema execution failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(g_db);
        g_db = NULL;
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    rc = sqlite3_exec(g_db, SEED_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB WARN] Seed execution note: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

void db_close(void) {
    pthread_mutex_lock(&g_db_lock);
    if (g_db != NULL) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    pthread_mutex_unlock(&g_db_lock);
}

int db_get_hospitals_json(char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\",\"hospitals\":[]}");
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, address, distance_km, icu_beds_total, icu_beds_available, "
                      "oxygen_percent, phone, is_emergency_active FROM hospitals ORDER BY distance_km ASC;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"hospitals\":[]}");
    }

    size_t offset = snprintf(buf, max_len, "{\"count\":0,\"hospitals\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char*)sqlite3_column_text(stmt, 1);
        const char *addr = (const char*)sqlite3_column_text(stmt, 2);
        double dist = sqlite3_column_double(stmt, 3);
        int icu_tot = sqlite3_column_int(stmt, 4);
        int icu_avail = sqlite3_column_int(stmt, 5);
        int o2 = sqlite3_column_int(stmt, 6);
        const char *phone = (const char*)sqlite3_column_text(stmt, 7);
        int active = sqlite3_column_int(stmt, 8);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"name\":\"%s\",\"address\":\"%s\",\"distance_km\":%.1f,"
            "\"icu_beds_total\":%d,\"icu_beds_available\":%d,\"oxygen_percent\":%d,"
            "\"phone\":\"%s\",\"is_emergency_active\":%s}",
            id, name ? name : "", addr ? addr : "", dist, icu_tot, icu_avail, o2,
            phone ? phone : "", active ? "true" : "false");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    if (count > 0 && buf[9] == '0') {
        buf[9] = '0' + (count % 10);
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

int db_get_doctors_json(char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\",\"doctors\":[]}");
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, speciality, experience_years, fee_usd, rating, photo_url FROM doctors ORDER BY id ASC;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"doctors\":[]}");
    }

    size_t offset = snprintf(buf, max_len, "{\"count\":0,\"doctors\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char*)sqlite3_column_text(stmt, 1);
        const char *spec = (const char*)sqlite3_column_text(stmt, 2);
        int exp = sqlite3_column_int(stmt, 3);
        int fee = sqlite3_column_int(stmt, 4);
        double rating = sqlite3_column_double(stmt, 5);
        const char *photo = (const char*)sqlite3_column_text(stmt, 6);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"name\":\"%s\",\"speciality\":\"%s\",\"experience\":%d,"
            "\"fee\":\"$%d\",\"rating\":%.1f,\"photo\":\"%s\"}",
            id, name ? name : "", spec ? spec : "", exp, fee, rating, photo ? photo : "");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    if (count > 0 && buf[9] == '0') {
        buf[9] = '0' + (count % 10);
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

int db_get_slots_json(int doctor_id, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\",\"slots\":[]}");
    }

    sqlite3_stmt *stmt;
    char sql[512];
    if (doctor_id > 0) {
        snprintf(sql, sizeof(sql),
            "SELECT s.id, s.doctor_id, d.name AS doctor_name, s.day_of_week, s.start_time, s.end_time, s.is_booked "
            "FROM appointment_slots s JOIN doctors d ON s.doctor_id = d.id "
            "WHERE s.doctor_id = %d ORDER BY s.id ASC;", doctor_id);
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT s.id, s.doctor_id, d.name AS doctor_name, s.day_of_week, s.start_time, s.end_time, s.is_booked "
            "FROM appointment_slots s JOIN doctors d ON s.doctor_id = d.id ORDER BY s.doctor_id ASC, s.id ASC;");
    }

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"slots\":[]}");
    }

    size_t offset = snprintf(buf, max_len, "{\"doctor_id\":%d,\"slots\":[", doctor_id);
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;

        int id = sqlite3_column_int(stmt, 0);
        int doc_id = sqlite3_column_int(stmt, 1);
        const char *dname = (const char*)sqlite3_column_text(stmt, 2);
        const char *day = (const char*)sqlite3_column_text(stmt, 3);
        const char *start = (const char*)sqlite3_column_text(stmt, 4);
        const char *end = (const char*)sqlite3_column_text(stmt, 5);
        int booked = sqlite3_column_int(stmt, 6);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"doctor_id\":%d,\"doctor_name\":\"%s\",\"day\":\"%s\","
            "\"start_time\":\"%s\",\"end_time\":\"%s\",\"slot\":\"%s: %s - %s\",\"available\":%s}",
            id, doc_id, dname ? dname : "", day ? day : "", start ? start : "", end ? end : "",
            day ? day : "", start ? start : "", end ? end : "", booked ? "false" : "true");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

int db_reset_slot_for_demo(int slot_id) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE appointment_slots SET is_booked = 0 WHERE id = ?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, slot_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

int db_reset_queue(void) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "DELETE FROM requests;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "DELETE FROM resource_allocations;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "UPDATE hospital_resources SET occupied_quantity = 0;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

int db_update_appointment_status(const char *booking_code, const char *status) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE appointments SET status = ? WHERE booking_code = ?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, booking_code, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_book_appointment(int doctor_id, int slot_id, const char *patient_name,
                          const char *patient_phone, const char *notes,
                          int worker_id, int turnaround_ms,
                          char *out_booking_code, size_t code_len,
                          char *err_msg, size_t err_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        snprintf(err_msg, err_len, "Database connection not initialized");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    int rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        snprintf(err_msg, err_len, "Failed to acquire database write lock");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    int target_slot_id = slot_id;
    if (target_slot_id <= 0 && doctor_id > 0) {
        sqlite3_stmt *find_stmt;
        const char *find_sql = "SELECT id FROM appointment_slots WHERE doctor_id = ? AND is_booked = 0 LIMIT 1;";
        if (sqlite3_prepare_v2(g_db, find_sql, -1, &find_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(find_stmt, 1, doctor_id);
            if (sqlite3_step(find_stmt) == SQLITE_ROW) {
                target_slot_id = sqlite3_column_int(find_stmt, 0);
            }
            sqlite3_finalize(find_stmt);
        }
        if (target_slot_id <= 0) {
            sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
            snprintf(err_msg, err_len, "No available slots for Doctor ID %d", doctor_id);
            pthread_mutex_unlock(&g_db_lock);
            return 1;
        }
    }

    sqlite3_stmt *check_stmt;
    const char *check_sql = "SELECT doctor_id, is_booked FROM appointment_slots WHERE id = ?;";
    int actual_doc_id = -1;
    int is_booked = -1;

    if (sqlite3_prepare_v2(g_db, check_sql, -1, &check_stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Database prepare failed");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }
    sqlite3_bind_int(check_stmt, 1, target_slot_id);

    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
        actual_doc_id = sqlite3_column_int(check_stmt, 0);
        is_booked = sqlite3_column_int(check_stmt, 1);
    } else {
        sqlite3_finalize(check_stmt);
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Invalid slot ID %d", target_slot_id);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_finalize(check_stmt);

    if (is_booked != 0) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Slot %d is already booked by another patient", target_slot_id);
        pthread_mutex_unlock(&g_db_lock);
        return 1;
    }

    if (doctor_id > 0 && doctor_id != actual_doc_id) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Slot %d does not belong to doctor ID %d", target_slot_id, doctor_id);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    doctor_id = actual_doc_id;

    sqlite3_stmt *upd_stmt;
    const char *upd_sql = "UPDATE appointment_slots SET is_booked = 1 WHERE id = ? AND is_booked = 0;";
    if (sqlite3_prepare_v2(g_db, upd_sql, -1, &upd_stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to prepare slot update");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }
    sqlite3_bind_int(upd_stmt, 1, target_slot_id);
    rc = sqlite3_step(upd_stmt);
    sqlite3_finalize(upd_stmt);

    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Slot was concurrently booked by another client");
        pthread_mutex_unlock(&g_db_lock);
        return 1;
    }

    long random_suffix = rand() % 90000 + 10000;
    char booking_code[64];
    snprintf(booking_code, sizeof(booking_code), "BK-%ld-%d", random_suffix, target_slot_id);
    if (out_booking_code) {
        snprintf(out_booking_code, code_len, "%s", booking_code);
    }

    sqlite3_stmt *ins_stmt;
    const char *ins_sql = "INSERT INTO appointments (booking_code, doctor_id, slot_id, patient_name, patient_phone, notes, status, handled_by_worker, turnaround_ms) "
                          "VALUES (?, ?, ?, ?, ?, ?, 'CONFIRMED', ?, ?);";
    if (sqlite3_prepare_v2(g_db, ins_sql, -1, &ins_stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to prepare appointment insert");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }
    sqlite3_bind_text(ins_stmt, 1, booking_code, -1, SQLITE_STATIC);
    sqlite3_bind_int(ins_stmt, 2, doctor_id);
    sqlite3_bind_int(ins_stmt, 3, target_slot_id);
    sqlite3_bind_text(ins_stmt, 4, patient_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_stmt, 5, patient_phone ? patient_phone : "N/A", -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_stmt, 6, notes ? notes : "", -1, SQLITE_STATIC);
    sqlite3_bind_int(ins_stmt, 7, worker_id);
    sqlite3_bind_int(ins_stmt, 8, turnaround_ms);

    rc = sqlite3_step(ins_stmt);
    sqlite3_finalize(ins_stmt);

    if (rc != SQLITE_DONE) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to persist appointment");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

int db_get_appointments_json(int limit, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\",\"appointments\":[]}");
    }

    if (limit <= 0 || limit > 100) limit = 20;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT a.id, a.booking_code, a.patient_name, a.patient_phone, d.name AS doctor_name, "
                      "d.speciality, s.day_of_week, s.start_time, s.end_time, a.status, a.created_at, "
                      "a.handled_by_worker, a.turnaround_ms "
                      "FROM appointments a "
                      "JOIN doctors d ON a.doctor_id = d.id "
                      "JOIN appointment_slots s ON a.slot_id = s.id "
                      "ORDER BY a.id DESC LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"appointments\":[]}");
    }
    sqlite3_bind_int(stmt, 1, limit);

    size_t offset = snprintf(buf, max_len, "{\"count\":0,\"appointments\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *code = (const char*)sqlite3_column_text(stmt, 1);
        const char *pname = (const char*)sqlite3_column_text(stmt, 2);
        const char *phone = (const char*)sqlite3_column_text(stmt, 3);
        const char *dname = (const char*)sqlite3_column_text(stmt, 4);
        const char *spec = (const char*)sqlite3_column_text(stmt, 5);
        const char *day = (const char*)sqlite3_column_text(stmt, 6);
        const char *start = (const char*)sqlite3_column_text(stmt, 7);
        const char *end = (const char*)sqlite3_column_text(stmt, 8);
        const char *status = (const char*)sqlite3_column_text(stmt, 9);
        const char *created = (const char*)sqlite3_column_text(stmt, 10);
        int worker = sqlite3_column_int(stmt, 11);
        int turnaround = sqlite3_column_int(stmt, 12);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"booking_code\":\"%s\",\"patient_name\":\"%s\",\"phone\":\"%s\",\"doctor\":\"%s\",\"speciality\":\"%s\",\"day\":\"%s\",\"time\":\"%s - %s\",\"status\":\"%s\",\"created_at\":\"%s\",\"handled_by_worker\":%d,\"turnaround_ms\":%d}",
            id, code ? code : "", pname ? pname : "", phone ? phone : "", dname ? dname : "", spec ? spec : "", day ? day : "", start ? start : "", end ? end : "", status ? status : "", created ? created : "", worker, turnaround);
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    char count_patch[32];
    int patch_len = snprintf(count_patch, sizeof(count_patch), "%d", count);
    if (buf[9] == '0' && patch_len == 1) {
        buf[9] = count_patch[0];
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

int db_get_appointment_by_id_json(int appointment_id, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\"}");
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT a.id, a.booking_code, a.patient_name, a.patient_phone, d.name, "
                      "d.speciality, s.day_of_week, s.start_time, s.end_time, a.status, a.created_at, "
                      "a.handled_by_worker, a.turnaround_ms "
                      "FROM appointments a "
                      "JOIN doctors d ON a.doctor_id = d.id "
                      "JOIN appointment_slots s ON a.slot_id = s.id "
                      "WHERE a.id = ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\"}");
    }
    sqlite3_bind_int(stmt, 1, appointment_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *code = (const char*)sqlite3_column_text(stmt, 1);
        const char *pname = (const char*)sqlite3_column_text(stmt, 2);
        const char *phone = (const char*)sqlite3_column_text(stmt, 3);
        const char *dname = (const char*)sqlite3_column_text(stmt, 4);
        const char *spec = (const char*)sqlite3_column_text(stmt, 5);
        const char *day = (const char*)sqlite3_column_text(stmt, 6);
        const char *start = (const char*)sqlite3_column_text(stmt, 7);
        const char *end = (const char*)sqlite3_column_text(stmt, 8);
        const char *status = (const char*)sqlite3_column_text(stmt, 9);
        const char *created = (const char*)sqlite3_column_text(stmt, 10);
        int worker = sqlite3_column_int(stmt, 11);
        int turnaround = sqlite3_column_int(stmt, 12);

        int len = snprintf(buf, max_len,
            "{\"appointment\":{\"id\":%d,\"booking_code\":\"%s\",\"patient_name\":\"%s\",\"phone\":\"%s\",\"doctor\":\"%s\",\"speciality\":\"%s\",\"day\":\"%s\",\"time\":\"%s - %s\",\"status\":\"%s\",\"created_at\":\"%s\",\"handled_by_worker\":%d,\"turnaround_ms\":%d}}",
            id, code ? code : "", pname ? pname : "", phone ? phone : "", dname ? dname : "", spec ? spec : "", day ? day : "", start ? start : "", end ? end : "", status ? status : "", created ? created : "", worker, turnaround);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&g_db_lock);
        return len;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return snprintf(buf, max_len, "{\"error\":\"Appointment not found\",\"id\":%d}", appointment_id);
}

int db_get_emergency_info_json(char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Database not initialized\"}");
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT SUM(icu_beds_total), SUM(icu_beds_available) FROM hospitals WHERE is_emergency_active = 1;";
    int total_icu = 0;
    int avail_icu = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total_icu = sqlite3_column_int(stmt, 0);
            avail_icu = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&g_db_lock);

    return snprintf(buf, max_len,
        "{\"helpline\":\"108 / 112\",\"emergency_status\":\"OPERATIONAL\",\"total_icu_beds\":%d,\"available_icu_beds\":%d,\"blood_bank\":\"A+, B+, O+ Available\",\"triage_categories\":[\"P0: Resuscitation/Trauma\",\"P1: Urgent Telemedicine\",\"P2: Vaccination Outbreak\",\"P3: Routine OPD Appointment\"]}",
        total_icu, avail_icu);
}

/* ================= PATIENTS & REQUESTS PERSISTENCE ================= */

int db_record_patient(const char *patient_id, const char *name, int age,
                      const char *gender, const char *phone) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO patients (patient_id, name, age, gender, phone) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, patient_id ? patient_id : "PAT-UNKNOWN", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name ? name : "Anonymous Patient", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, age > 0 ? age : 30);
    sqlite3_bind_text(stmt, 4, gender ? gender : "Other", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, phone ? phone : "N/A", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_record_request(const char *req_id, const char *patient_id, const char *patient_name,
                      int priority, const char *priority_name, const char *symptoms,
                      const char *vitals, const char *priority_reason, const char *required_resource) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO requests (request_id, patient_id, patient_name, priority, priority_name, "
                      "symptoms, vitals, priority_reason, required_resource, status) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'QUEUED');";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, req_id ? req_id : "REQ-UNKNOWN", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, patient_id ? patient_id : "PAT-UNKNOWN", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, patient_name ? patient_name : "Anonymous", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, priority);
    sqlite3_bind_text(stmt, 5, priority_name ? priority_name : "Routine", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, symptoms ? symptoms : "None", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, vitals ? vitals : "Stable", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, priority_reason ? priority_reason : "Standard", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, required_resource ? required_resource : "none", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_least_loaded_doctor_id(void) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return 0;
    }
    int loads[4] = {0, 0, 0, 0};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT worker_id, COUNT(*) FROM requests WHERE status != 'DISCHARGED' AND worker_id >= 0 AND worker_id < 4 GROUP BY worker_id;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int wid = sqlite3_column_int(stmt, 0);
            int cnt = sqlite3_column_int(stmt, 1);
            if (wid >= 0 && wid < 4) {
                loads[wid] = cnt;
            }
        }
        sqlite3_finalize(stmt);
    }
    int min_load = loads[0];
    int best_doc = 0;
    for (int i = 1; i < 4; i++) {
        if (loads[i] < min_load) {
            min_load = loads[i];
            best_doc = i;
        }
    }
    pthread_mutex_unlock(&g_db_lock);
    return best_doc;
}

int db_update_request_status(const char *req_id, const char *status, int worker_id, int queue_wait_ms) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = (worker_id >= 0) ?
        "UPDATE requests SET status = ?, worker_id = ?, queue_wait_ms = ? WHERE request_id = ?;" :
        "UPDATE requests SET status = ? WHERE request_id = ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    if (worker_id >= 0) {
        sqlite3_bind_text(stmt, 1, status ? status : "COMPLETED", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, worker_id);
        sqlite3_bind_int(stmt, 3, queue_wait_ms);
        sqlite3_bind_text(stmt, 4, req_id, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, 1, status ? status : "COMPLETED", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, req_id, -1, SQLITE_STATIC);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_requests_json(int limit, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"requests\":[]}");
    }

    if (limit <= 0 || limit > 100) limit = 50;

    sqlite3_stmt *stmt;
    // Priority order: active care first (discharged at bottom), highest priority (P0=0) first, recent arrivals top
    const char *sql = 
        "SELECT r.id, r.request_id, r.patient_id, r.patient_name, r.priority, r.priority_name, "
        "       r.symptoms, r.vitals, r.priority_reason, r.required_resource, r.status, "
        "       r.worker_id, r.queue_wait_ms, r.created_at, "
        "       (SELECT a.allocation_code FROM resource_allocations a WHERE a.patient_id = r.patient_id ORDER BY a.id DESC LIMIT 1) AS alloc_code "
        "FROM requests r "
        "ORDER BY CASE WHEN r.status = 'DISCHARGED' THEN 1 ELSE 0 END, r.priority ASC, r.id ASC "
        "LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"requests\":[]}");
    }
    sqlite3_bind_int(stmt, 1, limit);

    size_t offset = snprintf(buf, max_len, "{\"requests\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 512) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *req_id = (const char*)sqlite3_column_text(stmt, 1);
        const char *pat_id = (const char*)sqlite3_column_text(stmt, 2);
        const char *pname = (const char*)sqlite3_column_text(stmt, 3);
        int prio = sqlite3_column_int(stmt, 4);
        const char *pname_desc = (const char*)sqlite3_column_text(stmt, 5);
        const char *symptoms = (const char*)sqlite3_column_text(stmt, 6);
        const char *vitals = (const char*)sqlite3_column_text(stmt, 7);
        const char *reason = (const char*)sqlite3_column_text(stmt, 8);
        const char *res = (const char*)sqlite3_column_text(stmt, 9);
        const char *status = (const char*)sqlite3_column_text(stmt, 10);
        int worker = sqlite3_column_int(stmt, 11);
        int wait_ms = sqlite3_column_int(stmt, 12);
        const char *created = (const char*)sqlite3_column_text(stmt, 13);
                const char *alloc_code = (const char*)sqlite3_column_text(stmt, 14);

        char attending_doc[128];
        if (worker == 0) snprintf(attending_doc, sizeof(attending_doc), "Dr. Ananya Sharma (Trauma Core W1)");
        else if (worker == 1) snprintf(attending_doc, sizeof(attending_doc), "Dr. Rajesh Kulkarni (Surgery & ICU Core W2)");
        else if (worker == 2) snprintf(attending_doc, sizeof(attending_doc), "Dr. Priya Iyer (Emergency Medicine Core W3)");
        else if (worker == 3) snprintf(attending_doc, sizeof(attending_doc), "Dr. Arvind Menon (Cardiology Core W4)");
        else if (worker >= 4) snprintf(attending_doc, sizeof(attending_doc), "On-Call Rapid Response Core W%d", worker + 1);
        else snprintf(attending_doc, sizeof(attending_doc), "Dr. Ananya Sharma (Trauma Core W1)");

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"request_id\":\"%s\",\"patient_id\":\"%s\",\"patient_name\":\"%s\","
            "\"priority\":%d,\"priority_label\":\"%s\",\"symptoms\":\"%s\",\"vitals\":\"%s\","
            "\"priority_reason\":\"%s\",\"required_resource\":\"%s\",\"status\":\"%s\","
            "\"worker_id\":%d,\"worker_name\":\"Worker W%d\",\"attending_doctor\":\"%s\",\"queue_wait_ms\":%d,\"created_at\":\"%s\",\"allocation_code\":\"%s\"}",
            id, req_id ? req_id : "", pat_id ? pat_id : "", pname ? pname : "",
            prio, pname_desc ? pname_desc : "", symptoms ? symptoms : "", vitals ? vitals : "",
            reason ? reason : "", res ? res : "none", status ? status : "QUEUED",
            worker, (worker >= 0 ? worker + 1 : 1), attending_doc, wait_ms, created ? created : "",
            alloc_code ? alloc_code : "NONE");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    offset += snprintf(buf + offset, max_len - offset, "],\"count\":%d}", count);
    return (int)offset;
}

/* ================= HOSPITAL RESOURCES MANAGEMENT ================= */

int db_get_resources_json(char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "[]");
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, resource_type, display_name, total_quantity, occupied_quantity, unit FROM hospital_resources ORDER BY id ASC;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "[]");
    }

    size_t offset = snprintf(buf, max_len, "[");
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;

        int id = sqlite3_column_int(stmt, 0);
        const char *rtype = (const char*)sqlite3_column_text(stmt, 1);
        const char *dname = (const char*)sqlite3_column_text(stmt, 2);
        int total = sqlite3_column_int(stmt, 3);
        int occupied = sqlite3_column_int(stmt, 4);
        int available = total - occupied;
        if (available < 0) available = 0;
        const char *unit = (const char*)sqlite3_column_text(stmt, 5);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"resource_type\":\"%s\",\"display_name\":\"%s\",\"total\":%d,"
            "\"occupied\":%d,\"available\":%d,\"unit\":\"%s\"}",
            id, rtype ? rtype : "", dname ? dname : "", total, occupied, available, unit ? unit : "Units");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    offset += snprintf(buf + offset, max_len - offset, "]");
    return (int)offset;
}

int db_allocate_resource(const char *resource_type, const char *patient_id, const char *patient_name,
                         char *out_code, size_t code_len, char *err_msg, size_t err_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        snprintf(err_msg, err_len, "Database connection not initialized");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    if (!resource_type || strlen(resource_type) == 0 || strcmp(resource_type, "none") == 0) {
        pthread_mutex_unlock(&g_db_lock);
        return 0; // No resource required
    }

    int rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        snprintf(err_msg, err_len, "Failed to acquire transaction write lock for resource allocation");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    sqlite3_stmt *stmt;
    const char *check_sql = "SELECT total_quantity, occupied_quantity FROM hospital_resources WHERE resource_type = ?;";
    if (sqlite3_prepare_v2(g_db, check_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to prepare resource check");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }
    sqlite3_bind_text(stmt, 1, resource_type, -1, SQLITE_STATIC);

    int total = 0, occupied = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int(stmt, 0);
        occupied = sqlite3_column_int(stmt, 1);
    } else {
        sqlite3_finalize(stmt);
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Resource type '%s' not tracked in hospital inventory", resource_type);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    sqlite3_finalize(stmt);

    if (occupied >= total) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Capacity Full: No available '%s' (Occupied %d / %d). Mutex transaction rolled back.",
                 resource_type, occupied, total);
        pthread_mutex_unlock(&g_db_lock);
        return 1; // Shortage alert
    }

    // Allocate resource atomically
    sqlite3_stmt *upd_stmt;
    const char *upd_sql = "UPDATE hospital_resources SET occupied_quantity = occupied_quantity + 1 WHERE resource_type = ?;";
    if (sqlite3_prepare_v2(g_db, upd_sql, -1, &upd_stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to prepare resource increment");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }
    sqlite3_bind_text(upd_stmt, 1, resource_type, -1, SQLITE_STATIC);
    rc = sqlite3_step(upd_stmt);
    sqlite3_finalize(upd_stmt);

    if (rc != SQLITE_DONE) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Failed to update resource count");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    long rand_id = rand() % 90000 + 10000;
    char code[64];
    snprintf(code, sizeof(code), "RES-%ld", rand_id);
    if (out_code) snprintf(out_code, code_len, "%s", code);

    sqlite3_stmt *ins_stmt;
    const char *ins_sql = "INSERT INTO resource_allocations (allocation_code, patient_id, patient_name, resource_type, status) "
                          "VALUES (?, ?, ?, ?, 'ALLOCATED');";
    if (sqlite3_prepare_v2(g_db, ins_sql, -1, &ins_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ins_stmt, 1, code, -1, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 2, patient_id ? patient_id : "PAT-DEMO", -1, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 3, patient_name ? patient_name : "Demo Patient", -1, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 4, resource_type, -1, SQLITE_STATIC);
        sqlite3_step(ins_stmt);
        sqlite3_finalize(ins_stmt);
    }

    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
    return 0; // Success
}

int db_release_resource(const char *allocation_code, const char *resource_type,
                        char *err_msg, size_t err_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        snprintf(err_msg, err_len, "Database connection not initialized");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    int rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        snprintf(err_msg, err_len, "Failed to acquire lock for release");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    char actual_type[64] = {0};
    if (resource_type && strlen(resource_type) > 0) {
        snprintf(actual_type, sizeof(actual_type), "%s", resource_type);
    }

    if (allocation_code && strlen(allocation_code) > 0) {
        sqlite3_stmt *find_stmt;
        const char *find_sql = "SELECT resource_type FROM resource_allocations WHERE allocation_code = ? AND status = 'ALLOCATED';";
        if (sqlite3_prepare_v2(g_db, find_sql, -1, &find_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(find_stmt, 1, allocation_code, -1, SQLITE_STATIC);
            if (sqlite3_step(find_stmt) == SQLITE_ROW) {
                const char *r = (const char*)sqlite3_column_text(find_stmt, 0);
                if (r && strlen(actual_type) == 0) snprintf(actual_type, sizeof(actual_type), "%s", r);
            }
            sqlite3_finalize(find_stmt);
        }

        sqlite3_stmt *upd_alloc;
        const char *upd_alloc_sql = "UPDATE resource_allocations SET status = 'RELEASED', released_at = CURRENT_TIMESTAMP WHERE allocation_code = ?;";
        if (sqlite3_prepare_v2(g_db, upd_alloc_sql, -1, &upd_alloc, NULL) == SQLITE_OK) {
            sqlite3_bind_text(upd_alloc, 1, allocation_code, -1, SQLITE_STATIC);
            sqlite3_step(upd_alloc);
            sqlite3_finalize(upd_alloc);
        }
    }

    if (strlen(actual_type) > 0) {
        sqlite3_stmt *upd_res;
        const char *upd_res_sql = "UPDATE hospital_resources SET occupied_quantity = MAX(0, occupied_quantity - 1) WHERE resource_type = ?;";
        if (sqlite3_prepare_v2(g_db, upd_res_sql, -1, &upd_res, NULL) == SQLITE_OK) {
            sqlite3_bind_text(upd_res, 1, actual_type, -1, SQLITE_STATIC);
            sqlite3_step(upd_res);
            sqlite3_finalize(upd_res);
        }
    }

    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
    return 0;
}

int db_get_allocations_json(int limit, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"allocations\":[]}");
    }

    if (limit <= 0 || limit > 100) limit = 30;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, allocation_code, patient_id, patient_name, resource_type, status, allocated_at, released_at "
                      "FROM resource_allocations ORDER BY id DESC LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"allocations\":[]}");
    }
    sqlite3_bind_int(stmt, 1, limit);

    size_t offset = snprintf(buf, max_len, "{\"count\":0,\"allocations\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *code = (const char*)sqlite3_column_text(stmt, 1);
        const char *pid = (const char*)sqlite3_column_text(stmt, 2);
        const char *pname = (const char*)sqlite3_column_text(stmt, 3);
        const char *rtype = (const char*)sqlite3_column_text(stmt, 4);
        const char *status = (const char*)sqlite3_column_text(stmt, 5);
        const char *alloc_at = (const char*)sqlite3_column_text(stmt, 6);
        const char *rel_at = (const char*)sqlite3_column_text(stmt, 7);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"allocation_code\":\"%s\",\"patient_id\":\"%s\",\"patient_name\":\"%s\","
            "\"resource_type\":\"%s\",\"status\":\"%s\",\"allocated_at\":\"%s\",\"released_at\":\"%s\"}",
            id, code ? code : "", pid ? pid : "", pname ? pname : "",
            rtype ? rtype : "", status ? status : "", alloc_at ? alloc_at : "", rel_at ? rel_at : "None");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    char count_patch[32];
    int patch_len = snprintf(count_patch, sizeof(count_patch), "%d", count);
    if (buf[9] == '0' && patch_len == 1) {
        buf[9] = count_patch[0];
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

/* ================= SYSTEM EVENTS / LOG STREAM ================= */

int db_log_event(const char *event_type, const char *severity,
                 const char *message, const char *related_id) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO system_events (event_type, severity, message, related_id) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, event_type ? event_type : "INFO", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, severity ? severity : "INFO", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message ? message : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, related_id ? related_id : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_events_json(int limit, char *buf, size_t max_len) {
    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"events\":[]}");
    }

    if (limit <= 0 || limit > 100) limit = 30;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, event_type, severity, message, related_id, created_at FROM system_events ORDER BY id DESC LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"count\":0,\"events\":[]}");
    }
    sqlite3_bind_int(stmt, 1, limit);

    size_t offset = snprintf(buf, max_len, "{\"count\":0,\"events\":[");
    bool first = true;
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 250) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;
        count++;

        int id = sqlite3_column_int(stmt, 0);
        const char *etype = (const char*)sqlite3_column_text(stmt, 1);
        const char *sev = (const char*)sqlite3_column_text(stmt, 2);
        const char *msg = (const char*)sqlite3_column_text(stmt, 3);
        const char *rel = (const char*)sqlite3_column_text(stmt, 4);
        const char *created = (const char*)sqlite3_column_text(stmt, 5);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"event_type\":\"%s\",\"severity\":\"%s\",\"message\":\"%s\",\"related_id\":\"%s\",\"created_at\":\"%s\"}",
            id, etype ? etype : "", sev ? sev : "", msg ? msg : "", rel ? rel : "", created ? created : "");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    char count_patch[32];
    int patch_len = snprintf(count_patch, sizeof(count_patch), "%d", count);
    if (buf[9] == '0' && patch_len == 1) {
        buf[9] = count_patch[0];
    }

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}
