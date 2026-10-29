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
    ");";

static const char *SEED_SQL =
    "INSERT INTO hospitals (name, address, distance_km, icu_beds_total, icu_beds_available, oxygen_percent, phone, is_emergency_active)"
    " VALUES"
    " ('City General Hospital', 'Rua Sao Salvador 57, Sector 4', 1.2, 20, 14, 100, '+1-800-555-0101', 1),"
    " ('Apollo Emergency Care', 'Largo Prazeres 120, Central', 3.5, 16, 8, 98, '+1-800-555-0102', 1),"
    " ('Metro Trauma Center', 'R Velhas 92, South Avenue', 5.1, 30, 22, 95, '+1-800-555-0103', 1);"
    "INSERT INTO doctors (id, name, speciality, experience_years, fee_usd, rating, photo_url)"
    " VALUES"
    " (1, 'Dr. Ananya Sharma', 'Pediatric', 12, 500, 4.9, 'https://images.unsplash.com/photo-1594824813589-9a7457788e34?w=100&auto=format&fit=crop&q=80'),"
    " (2, 'Dr. Rajesh Kulkarni', 'Surgical', 10, 800, 4.8, 'https://images.unsplash.com/photo-1622253692010-333f2da6031d?w=100&auto=format&fit=crop&q=80'),"
    " (3, 'Dr. Priya Iyer', 'Gastroenterology', 7, 400, 4.9, 'https://images.unsplash.com/photo-1559839734-2b71ea197ec2?w=100&auto=format&fit=crop&q=80'),"
    " (4, 'Dr. Arvind Menon', 'Cardiology', 15, 1000, 5.0, 'https://images.unsplash.com/photo-1537368910025-700350fe46c7?w=100&auto=format&fit=crop&q=80');"
    "INSERT INTO appointment_slots (doctor_id, day_of_week, start_time, end_time, is_booked)"
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
    " (4, 'Sat', '02:00 PM', '02:30 PM', 0);";

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

    sqlite3_busy_timeout(g_db, 5000);
    char *err_msg = NULL;
    rc = sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
    }

    rc = sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Schema setup failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    // Check if doctors table has records; if not, seed default demo records
    sqlite3_stmt *stmt;
    int doctor_count = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM doctors;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            doctor_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (doctor_count == 0) {
        rc = sqlite3_exec(g_db, SEED_SQL, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[DB ERROR] Seed data insertion failed: %s\n", err_msg);
            sqlite3_free(err_msg);
        } else {
            printf("[DB] Seeded initial demonstration records successfully.\n");
        }
    }

    pthread_mutex_unlock(&g_db_lock);
    printf("[DB] Initialized SQLite database (%s).\n", path);
    return 0;
}

void db_close(void) {
    pthread_mutex_lock(&g_db_lock);
    if (g_db) {
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
    const char *sql = "SELECT id, name, address, distance_km, icu_beds_total, icu_beds_available, oxygen_percent, phone FROM hospitals ORDER BY distance_km ASC;";
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"hospitals\":[]}");
    }

    size_t offset = snprintf(buf, max_len, "{\"hospitals\":[");
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 150) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;

        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char*)sqlite3_column_text(stmt, 1);
        const char *addr = (const char*)sqlite3_column_text(stmt, 2);
        double dist = sqlite3_column_double(stmt, 3);
        int total = sqlite3_column_int(stmt, 4);
        int avail = sqlite3_column_int(stmt, 5);
        int oxy = sqlite3_column_int(stmt, 6);
        const char *phone = (const char*)sqlite3_column_text(stmt, 7);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"name\":\"%s\",\"address\":\"%s\",\"distance\":\"%.1f km\",\"distance_km\":%.1f,\"icu_beds_total\":%d,\"icu_beds\":%d,\"icu_beds_available\":%d,\"oxygen\":\"%d%%\",\"oxygen_percent\":%d,\"phone\":\"%s\"}",
            id, name ? name : "", addr ? addr : "", dist, dist, total, avail, avail, oxy, oxy, phone ? phone : "");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

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
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"doctors\":[]}");
    }

    size_t offset = snprintf(buf, max_len, "{\"doctors\":[");
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 150) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;

        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char*)sqlite3_column_text(stmt, 1);
        const char *spec = (const char*)sqlite3_column_text(stmt, 2);
        int exp = sqlite3_column_int(stmt, 3);
        int fee = sqlite3_column_int(stmt, 4);
        double rating = sqlite3_column_double(stmt, 5);
        const char *photo = (const char*)sqlite3_column_text(stmt, 6);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"name\":\"%s\",\"speciality\":\"%s\",\"experience\":\"%d yrs exp\",\"experience_years\":%d,\"fee\":\"₹%d\",\"fee_usd\":%d,\"rating\":%.1f,\"photo_url\":\"%s\"}",
            id, name ? name : "", spec ? spec : "", exp, exp, fee, fee, rating, photo ? photo : "");
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

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
    const char *sql;
    if (doctor_id > 0) {
        sql = "SELECT s.id, s.doctor_id, d.name, d.speciality, s.day_of_week, s.start_time, s.end_time, s.is_booked "
              "FROM appointment_slots s JOIN doctors d ON s.doctor_id = d.id "
              "WHERE s.doctor_id = ? ORDER BY s.id ASC;";
    } else {
        sql = "SELECT s.id, s.doctor_id, d.name, d.speciality, s.day_of_week, s.start_time, s.end_time, s.is_booked "
              "FROM appointment_slots s JOIN doctors d ON s.doctor_id = d.id "
              "ORDER BY s.doctor_id ASC, s.id ASC;";
    }

    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return snprintf(buf, max_len, "{\"error\":\"Query prepare failed\",\"slots\":[]}");
    }

    if (doctor_id > 0) {
        sqlite3_bind_int(stmt, 1, doctor_id);
    }

    size_t offset = snprintf(buf, max_len, "{\"slots\":[");
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW && offset < max_len - 150) {
        if (!first) offset += snprintf(buf + offset, max_len - offset, ",");
        first = false;

        int slot_id = sqlite3_column_int(stmt, 0);
        int doc_id = sqlite3_column_int(stmt, 1);
        const char *doc_name = (const char*)sqlite3_column_text(stmt, 2);
        const char *spec = (const char*)sqlite3_column_text(stmt, 3);
        const char *day = (const char*)sqlite3_column_text(stmt, 4);
        const char *start = (const char*)sqlite3_column_text(stmt, 5);
        const char *end = (const char*)sqlite3_column_text(stmt, 6);
        int is_booked = sqlite3_column_int(stmt, 7);

        offset += snprintf(buf + offset, max_len - offset,
            "{\"id\":%d,\"doctor_id\":%d,\"doctor\":\"%s\",\"speciality\":\"%s\",\"day\":\"%s\",\"start_time\":\"%s\",\"end_time\":\"%s\",\"slot\":\"%s %s - %s\",\"available\":%s}",
            slot_id, doc_id, doc_name ? doc_name : "", spec ? spec : "", day ? day : "", start ? start : "", end ? end : "", day ? day : "", start ? start : "", end ? end : "", (is_booked == 0 ? "true" : "false"));
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    offset += snprintf(buf + offset, max_len - offset, "]}");
    return (int)offset;
}

int db_book_appointment(int doctor_id, int slot_id, const char *patient_name,
                          const char *patient_phone, const char *notes,
                          int worker_id, int turnaround_ms,
                          char *out_booking_code, size_t code_len,
                          char *err_msg, size_t err_len) {
    if (!patient_name || strlen(patient_name) == 0) {
        snprintf(err_msg, err_len, "Patient name is required");
        return -1;
    }

    pthread_mutex_lock(&g_db_lock);
    if (!g_db) {
        snprintf(err_msg, err_len, "Database not available");
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    char *errmsg = NULL;
    int rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        snprintf(err_msg, err_len, "Failed to begin transaction: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        pthread_mutex_unlock(&g_db_lock);
        return -2;
    }

    // If slot_id is not specified (e.g. <= 0), pick the first available slot for doctor_id
    int target_slot_id = slot_id;
    if (target_slot_id <= 0) {
        sqlite3_stmt *find_slot_stmt;
        const char *find_sql = (doctor_id > 0)
            ? "SELECT id FROM appointment_slots WHERE doctor_id = ? AND is_booked = 0 ORDER BY id ASC LIMIT 1;"
            : "SELECT id, doctor_id FROM appointment_slots WHERE is_booked = 0 ORDER BY id ASC LIMIT 1;";

        if (sqlite3_prepare_v2(g_db, find_sql, -1, &find_slot_stmt, NULL) == SQLITE_OK) {
            if (doctor_id > 0) sqlite3_bind_int(find_slot_stmt, 1, doctor_id);
            if (sqlite3_step(find_slot_stmt) == SQLITE_ROW) {
                target_slot_id = sqlite3_column_int(find_slot_stmt, 0);
                if (doctor_id <= 0) doctor_id = sqlite3_column_int(find_slot_stmt, 1);
            }
            sqlite3_finalize(find_slot_stmt);
        }
    }

    if (target_slot_id <= 0) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "No available slots found for requested doctor");
        pthread_mutex_unlock(&g_db_lock);
        return 1; // 409 Conflict
    }

    // Verify slot exists and check is_booked
    sqlite3_stmt *check_stmt;
    int actual_doc_id = 0;
    int is_booked = 1;
    const char *check_sql = "SELECT doctor_id, is_booked FROM appointment_slots WHERE id = ?;";
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
        return -1; // 400 Bad Request
    }
    sqlite3_finalize(check_stmt);

    if (is_booked != 0) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Slot %d is already booked by another patient", target_slot_id);
        pthread_mutex_unlock(&g_db_lock);
        return 1; // 409 Conflict
    }

    if (doctor_id > 0 && doctor_id != actual_doc_id) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        snprintf(err_msg, err_len, "Slot %d does not belong to doctor ID %d", target_slot_id, doctor_id);
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }
    doctor_id = actual_doc_id;

    // Mark slot as booked
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
        return 1; // 409 Conflict
    }

    // Generate unique booking code
    long random_suffix = rand() % 90000 + 10000;
    char booking_code[64];
    snprintf(booking_code, sizeof(booking_code), "BK-%ld-%d", random_suffix, target_slot_id);
    if (out_booking_code) {
        snprintf(out_booking_code, code_len, "%s", booking_code);
    }

    // Insert into appointments table
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
    return 0; // Success
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

    // Patch count in header if buffer allows
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
